#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "protocole.h" // contient la cle et la structure d'un message

int idQ, idShm;
int fd;
char* memPub;
void handlerSIGTERM(int sig);
int main()
{
  // Armement des signaux
  signal(SIGTERM, handlerSIGTERM);
  // Masquage de SIGINT
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGUSR1);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  fprintf(stderr, "(PUBLICITE %d) Recuperation de l'id de la file de messages\n", getpid());
    idQ = msgget(CLE, 0);
    if (idQ == -1)
    {
        perror("Erreur de la recup de l'id de la file");
        exit(1);
    }

    // Récupération de l'identifiant de la mémoire partagée
    fprintf(stderr, "(PUBLICITE %d) Recuperation de l'id de la mémoire partagée\n", getpid());
    idShm = shmget(CLE, 200, 0);
    if (idShm == -1)
    {
        perror("Erreur de la Recuperation de l'id de la mem partagee");
        exit(1);
    }

    // Attachement à la mémoire partagée
    memPub = (char*)shmat(idShm, NULL, 0);
    if (memPub == (char*)-1)
    {
        perror("Erreur d'attachement a la mem partagee");
        exit(1);
    }

    // Ouverture du fichier publicites.dat
    fd = open("publicites.dat", O_RDONLY);
    if (fd == -1)
    {
        perror("Erreur d'ouverture du fichier");
        exit(1);
    }

    fprintf(stderr, "(PUBLICITE %d) Debut boucle de publicites\n", getpid());

    while (1)
    {
        PUBLICITE pub;
        
        if (read(fd, &pub, sizeof(PUBLICITE)) == 0)
        {
            // Fin de fichier
            lseek(fd, 0, SEEK_SET); // on recommence
        }
        else if (read(fd, &pub, sizeof(PUBLICITE)) < 0)
        {
            perror("Erreur lors de la lecture du fichier");
            exit(1);
        }

        // Copier la publicité dans la mémoire partagée (200 chars)
        strcpy(memPub, pub.texte);

        // Envoi UPDATE_PUB au serveur
        MESSAGE m;
        m.type = 1;                // type quelconque (non utilisé)
        m.expediteur = getpid();
        m.requete = UPDATE_PUB;

        if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
        {
            perror("Erreur lors de l'envoie de la requete UPDATE_PUB");
            pause();
        }

        // Attente du nombre de secondes indiqué
        if (pub.nbSecondes > 0)
            sleep(pub.nbSecondes);
    }

    return 0;
}
void handlerSIGUSR1(int sig)
{
  fprintf(stderr, "(PUBLICITE %d) Nouvelle publicite !\n", getpid());

  // Lecture message NEW_PUB

  // Mise en place de la publicité en mémoire partagée
}
void handlerSIGTERM(int sig)
{
  fprintf(stderr, "(PUBLICITE %d) SIGTERM reçu, arrêt\n", getpid());
  close(fd);
  shmdt(memPub);
  exit(0);
}