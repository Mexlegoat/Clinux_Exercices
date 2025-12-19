#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <mysql.h>
#include <setjmp.h>
#include "protocole.h" // contient la cle et la structure d'un message
#include "FichierUtilisateur.h"

int idQ,idShm,idSem;
int fdPipe[2];
//ID DES FILS
int idFilsPub;
int pidModif, idFilsConsult;
sigjmp_buf jumpBuffer;
TAB_CONNEXIONS *tab;
void HandlerSIGCHLD(int sig);
void HandlerSIGINT(int sig);
void afficheTab();

MYSQL* connexion;


int main()
{
  // Connection à la BD
  connexion = mysql_init(NULL);
  if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
    exit(1);  
  }

  // Armement des signaux
  struct sigaction A;

  A.sa_handler = HandlerSIGINT;
  sigemptyset(&A.sa_mask);
  A.sa_flags = 0;

  if ((sigaction(SIGINT, &A, NULL)) == -1)
  {
    perror("(SERVEUR) Erreur lors de l'armement de SIGINT\n");
    exit(1);
  }
  printf("(SERVEUR) Le signal SINGINT à bien été armé \n");

  struct sigaction C;

  C.sa_handler = HandlerSIGCHLD;
  sigemptyset(&C.sa_mask);
  C.sa_flags = 0;

  if ((sigaction(SIGCHLD, &C, NULL)) == -1)
  {
    perror("(SERVEUR) Erreur lors de l'armement de SIGCHLD1\n");
    exit(1);
  }
  printf("(SERVEUR) Le signal SIGCHLD à bien été armé \n");
  // Creation des ressources
  fprintf(stderr,"(SERVEUR %d) Creation de la file de messages\n",getpid());
  if ((idQ = msgget(CLE, IPC_CREAT | 0600)) == -1)  // CLE definie dans protocole.h
  {
    perror("(SERVEUR) Erreur de msgget");
    exit(EXIT_FAILURE);
  }
  ;
  if ((idShm = shmget(CLE, 200, IPC_CREAT | 0600)) == -1) {
    perror("(SERVEUR) Erreur de shmget");
    msgctl(idQ, IPC_RMID, NULL);
    exit(EXIT_FAILURE);
  }
  idSem = semget(CLE, 1, IPC_CREAT | 0600);
  if (idSem == -1)
  {
    perror("(SERVEUR) Erreur semget");
    exit(1);
  }
  union semun
  {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
  };

  union semun arg;
  arg.val = 1;

  if (semctl(idSem, 0, SETVAL, arg) == -1)
  {
      perror("(SERVEUR) Erreur semctl SETVAL");
      exit(1);
  }

  fprintf(stderr, "(SERVEUR) Sémaphore créé et initialisé à 1\n");    
  fprintf(stderr,"(SERVEUR %d) memoire partagee cree idShm=%d\n", getpid(), idShm);
  // Initialisation du tableau de connexions
  fprintf(stderr,"(SERVEUR %d) Initialisation de la table des connexions\n",getpid());
  tab = (TAB_CONNEXIONS*) malloc(sizeof(TAB_CONNEXIONS)); 

  for (int i=0 ; i<6 ; i++)
  {
    tab->connexions[i].pidFenetre = 0;
    strcpy(tab->connexions[i].nom,"");
    for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
    tab->connexions[i].pidModification = 0;
  }
  tab->pidServeur1 = getpid();
  tab->pidServeur2 = 0;
  tab->pidAdmin = 0;
  tab->pidPublicite = 0;

  afficheTab();
  // Creation du processus Publicite

  int i,k,j, pid;
  MESSAGE m;
  MESSAGE reponse;
  char requete[200];
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  PUBLICITE pub;
  pid = fork();
  if (pid == 0) 
  {
    execl("./Publicite", "./Publicite", NULL);
  }
  tab->pidPublicite = pid;
  while(1)
  {
    bool isFalse = false;
    int temp = -1;      
    int ABSENT = 0;
    i = 0;
    int pos;
    const char* NVCLIENT = "1";
    char deconnecte[50];
    int position;
  	fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),1,0) == -1)
    {
      if (errno == EINTR)
      {
          // Interruption par signal : PAS une erreur
          continue;   // boucle
      }
      perror("(SERVEUR) Erreur de msgrcv ");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }
    switch(m.requete)
    {
      case CONNECT :  
                      fprintf(stderr, "(SERVEUR %d) Requete CONNECT reçue de %d\n", getpid(), m.expediteur);
                      for (int i = 0; i < 6; i++)
                      {
                        if (tab->connexions[i].pidFenetre == 0)
                        {
                          tab->connexions[i].pidFenetre = m.expediteur;
                          break;
                        }
                      }
                      fprintf(stderr, "(SERVEUR) Trop de client");
                      break;

      case DECONNECT :  
                      fprintf(stderr, "(SERVEUR %d) Requete DECONNECT reçue de %d\n", getpid(), m.expediteur);
                      i = 0;
                      while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur)
                       { i++;}
                      tab->connexions[i].pidFenetre = 0;
                      strcpy(tab->connexions[i].nom,"");

                      break;

      case LOGIN :  
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN reçue de %d : --%s--%s--%s--\n",
                              getpid(), m.expediteur, m.data1, m.data2, m.texte);

                      MESSAGE reponse;
                      position = estPresent(m.data2);

                      if (strcmp(m.data1, NVCLIENT) == 0) // Nouveau client
                      {
                          if (position == 0) // absent
                          {
                              strcpy(reponse.data1, "1");
                              ajouteUtilisateur(m.data2, m.texte);
                              strcpy(reponse.texte, "Utilisateur ajouté\n");
                              i = 0;
                              sprintf(requete,"insert into UNIX_FINAL values (NULL,'%s','%s','%s');",m.data2, "---", "---");
                              mysql_query(connexion,requete);
                              while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                              tab->connexions[i].nom[0] = '\0';
                              strcpy(tab->connexions[i].nom, m.data2);
                              // Notifier tous les clients existants
                              for (int k = 0; k < 6; k++)
                              {
                                  if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                                  {
                                      MESSAGE add;
                                      add.type = tab->connexions[k].pidFenetre;
                                      add.expediteur = getpid();
                                      add.requete = ADD_USER;
                                      strcpy(add.data1, m.data2); // le nouveau nom
                                      msgsnd(idQ, &add, sizeof(MESSAGE)-sizeof(long), 0);
                                      kill(tab->connexions[k].pidFenetre, SIGUSR1);
                                  }
                              }
                              for (int k = 0; k < 6; k++)
                              {
                                  if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                                  {
                                      MESSAGE add;
                                      add.type = m.expediteur;        // nouveau client
                                      add.expediteur = getpid();
                                      add.requete = ADD_USER;
                                      strcpy(add.data1, tab->connexions[k].nom); // utilisateur déjà connecté
                                      msgsnd(idQ, &add, sizeof(MESSAGE)-sizeof(long), 0);
                                      kill(m.expediteur, SIGUSR1);

                                  }
                              }
                          }
                          else
                          {
                              strcpy(reponse.texte, "Utilisateur déjà existant!\n");
                              strcpy(reponse.data1, "0");
                          }
                      }
                      else // Client existant
                      {
                          if (position == 0)
                          {
                              strcpy(reponse.data1, "0");
                              strcpy(reponse.texte, "Utilisateur inexistant!\n");
                          }
                          else if (!verifieMotDePasse(position, m.texte))
                          {
                              strcpy(reponse.data1, "0");
                              strcpy(reponse.texte, "Mot de passe incorrect!\n");
                          }
                          else
                          {
                              // Ajouter le client dans le tableau
                              int i = 0;
                              while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                              pos = i;
                              i = 0;
                              while (i < 6 && strcmp(tab->connexions[i].nom, m.data2) != 0) i++; // on regarde si l'utilisateur est deja connecté
                              if (i < 6 && strcmp(tab->connexions[i].nom, m.data2) == 0) // une condition pour le précédent 
                              {
                                isFalse = true;
                              }
                              if (!isFalse)
                              {
                                strcpy(tab->connexions[pos].nom, m.data2);
                                strcpy(reponse.data1, "1");
                                strcpy(reponse.texte, "Vous êtes connectés!\n");
                                i = 0;
                                while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                                tab->connexions[i].nom[0] = '\0';
                                strcpy(tab->connexions[i].nom, m.data2);

                                // Envoyer la liste des utilisateurs déjà connectés au nouveau client
                                for (int k = 0; k < 6; k++)
                                {
                                    if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                                    {
                                        MESSAGE add;
                                        add.type = tab->connexions[k].pidFenetre;
                                        add.expediteur = getpid();
                                        add.requete = ADD_USER;
                                        strcpy(add.data1, m.data2); // le nouveau nom
                                        msgsnd(idQ, &add, sizeof(MESSAGE)-sizeof(long), 0);
                                        kill(tab->connexions[k].pidFenetre, SIGUSR1);
                                    }
                                }
                                for (int k = 0; k < 6; k++)
                                {
                                    if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                                    {
                                        MESSAGE add;
                                        add.type = m.expediteur;        // nouveau client
                                        add.expediteur = getpid();
                                        add.requete = ADD_USER;
                                        strcpy(add.data1, tab->connexions[k].nom); // utilisateur déjà connecté
                                        msgsnd(idQ, &add, sizeof(MESSAGE)-sizeof(long), 0);
                                        kill(m.expediteur, SIGUSR1);
                                    }
                                }
                              }
                              else
                              {
                                strcpy(reponse.data1, "0");
                                strcpy(reponse.texte, "L'utilisateur est deja connecté");
                              }
                          }
                      }
                      // Envoyer réponse LOGIN au client
                      reponse.type = m.expediteur;
                      reponse.expediteur = getpid();
                      reponse.requete = LOGIN;
                      strcpy(reponse.data2, m.data2);
                      if (msgsnd(idQ, &reponse, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                          perror("(CLIENT) Envoi du login");
                          msgctl(idQ, IPC_RMID, NULL);
                          exit(1);
                      }
                      kill(m.expediteur, SIGUSR1);
                      break;

      case LOGOUT :  
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT reçue de %d\n",getpid(),m.expediteur);
                      i = 0;
                      while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur)
                        i++;
                      strcpy(deconnecte, tab->connexions[i].nom);
                      strcpy(tab->connexions[i].nom,"");
                      for (int k = 0; k < 6; k++) 
                      {
                        if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur) 
                        {
                          MESSAGE notif;
                          notif.type = tab->connexions[k].pidFenetre;
                          notif.expediteur = getpid();
                          notif.requete = REMOVE_USER; // Requête pour notifier qu'un utilisateur s'est déconnecté
                          strcpy(notif.data1, deconnecte); // Nom de l'utilisateur qui s'est déconnecté
                          if (msgsnd(idQ, &notif, sizeof(MESSAGE) - sizeof(long), 0) == -1) 
                          {
                              perror("(SERVEUR) Erreur envoi DELETE_USER");
                          }
                          kill(tab->connexions[k].pidFenetre, SIGUSR1); // Notifier le client
                        }
                        for (int j = 0; j < 5; j++) 
                        {
                            if (tab->connexions[k].autres[j] != 0) 
                            {
                                tab->connexions[k].autres[j] = 0;
                            }
                        }
                      }
                      break;

      case ACCEPT_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete ACCEPT_USER reçue de %d\n",getpid(),m.expediteur);
                      MESSAGE r;  
                      for (i = 0; i < 6; i++)
                      {
                        if (strcmp(tab->connexions[i].nom, m.data1) == 0)
                        {
                          r.expediteur = tab->connexions[i].pidFenetre;
                          break;
                        }
                      }
                      if (strlen(m.data1) > 0)
                      {
                        for (int i = 0; i < 6; i++) 
                        {
                          if (tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre == m.expediteur) 
                          {
                              // Chercher une case libre dans "autres[]" de ce client
                              for (int j = 0; j < 5; j++) 
                              {
                                  if (tab->connexions[i].autres[j] == 0 ) 
                                  {
                                      tab->connexions[i].autres[j] = r.expediteur; // ajouter PID du client accepté
                                      fprintf(stderr,"(SERVEUR) PID %d ajouté à tab->connexions[%d].autres[%d]\n",
                                              m.expediteur, i, j);
                                      break; // on remplit qu'une seule case
                                  }
                              }
                          }
                        }
                        
                      }
                      break;

      case REFUSE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete REFUSE_USER reçue de %d\n",getpid(),m.expediteur);
                      for (int i = 0; i < 6; i++)
                      {
                        if (strcmp(tab->connexions[i].nom, m.data1) == 0)
                        {
                            r.expediteur = tab->connexions[i].pidFenetre;
                            break;
                        }
                      }
                      for (int i = 0; i < 6; i++) 
                      {
                        if (tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre == m.expediteur) 
                        {
                            // Chercher une case libre dans "autres[]" de ce client
                            for (int j = 0; j < 5; j++) 
                            {
                                if (tab->connexions[i].autres[j] == r.expediteur) 
                                {
                                    tab->connexions[i].autres[j] = 0; // ajouter PID du client accepté
                                    break; // on remplit qu'une seule case
                                }
                            }
                        }
                      }
                      break;

      case SEND :  
                      fprintf(stderr,"(SERVEUR %d) Requete SEND reçue de %d\n",getpid(),m.expediteur);
                      MESSAGE tmp;
                      for (int i = 0; i < 6; i++) 
                      {
                          if (tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre == m.expediteur) 
                          {
                            // Chercher les cases qui ne sont pas vides
                            for (int j = 0; j < 5; j++) 
                            {
                                m.type = tab->connexions[i].autres[j];
                                if (m.type != 0)
                                {
                                  strcpy(tmp.data1, tab->connexions[i].nom);
                                  temp = 1;
                                  for(k = 0; k < 6; k++)
                                  {
                                    if (tab->connexions[k].pidFenetre == m.type)
                                    {
                                      strcpy(m.data1, tab->connexions[i].nom);
                                      m.requete = SEND;
                                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                                      {
                                        perror("(CLIENT) Erreur d'envoie de la requete SEND");
                                        exit(1);
                                      }
                                      kill(m.type, SIGUSR1);
                                      break;         
                                    }
                                  }
                                }
                            }
                            break;
                          }
                      }
                      if (temp == 1) // n'envoyer qu'une seule fois le message vers celui qui a envoyé le message
                      {  
                        tmp = m;
                        tmp.requete = SEND;
                        tmp.type = m.expediteur;
                        if (msgsnd(idQ, &tmp, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        {
                          perror("(CLIENT) Erreur d'envoie de la requete SEND");
                          exit(1);
                        }
                        kill(tmp.type, SIGUSR1);
                      }
                      break; 

      case UPDATE_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete UPDATE_PUB reçue de %d\n",getpid(),m.expediteur);
                      for (int i = 0; i < 6; i++)
                      {
                        if(tab->connexions[i].pidFenetre != 0)
                        {
                          kill(tab->connexions[i].pidFenetre, SIGUSR2);
                        }
                      }
                      break;

      case CONSULT :
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);

                      idFilsConsult = fork();
                      if(idFilsConsult == 0)
                      {
                        execl("./Consultation", "Consultation", NULL);
                        perror("execl Consultation");
                        exit(1);
                      }

                      m.type = idFilsConsult;
                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                          perror("(SERVEUR) Erreur envoi CONSULT");
                          exit(1);
                      }
                      break;

      case MODIF1 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF1 reçue de %d\n",getpid(),m.expediteur);
                      // trouver l'utilisateur
                      i = 0;
                      while (i < 6 && tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre != m.expediteur)
                          i++;

                      // sécurité
                      if (i == 6) break;
                      if (tab->connexions[i].pidModification != 0)
                      {
                          fprintf(stderr,
                            "(SERVEUR) MODIF1 ignorée : déjà en cours\n");
                          break;
                      }

                      pidModif = fork();
                      if (pidModif == 0)
                      {
                          execl("./Modification", "Modification", NULL);
                          perror("execl Modification");
                          exit(1);
                      }
                      tab->connexions[i].pidModification = pidModif;
                      strcpy(m.data1, tab->connexions[i].nom);
                      m.type = pidModif;
                      // Transmettre la requête au processus Modification
                      if (msgsnd(idQ, &m, sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                          perror("msgsnd MODIF1");
                          exit(1);
                      }
                      break;

      case MODIF2 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF2 reçue de %d\n",getpid(),m.expediteur);
                      i = 0;
                      while (i < 6 &&
                             tab->connexions[i].pidFenetre != 0 &&
                             tab->connexions[i].pidFenetre != m.expediteur)
                          i++;

                      if (i == 6)
                          break;

                      pidModif = tab->connexions[i].pidModification;
                      if (pidModif == 0)
                      {
                          fprintf(stderr,"(SERVEUR) MODIF2 sans MODIF1\n");
                          break;
                      }

                      m.type = pidModif;
                      if (msgsnd(idQ, &m, sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                          perror("msgsnd MODIF2");
                          exit(1);
                      }
                      break;

      case LOGIN_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGOUT_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case NEW_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_USER reçue de %d : --%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2);
                      break;

      case DELETE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete DELETE_USER reçue de %d : --%s--\n",getpid(),m.expediteur,m.data1);
                      break;

      case NEW_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);
                      break;
    }
    afficheTab();
  }
}

void afficheTab()
{
  fprintf(stderr,"Pid Serveur 1 : %d\n",tab->pidServeur1);
  fprintf(stderr,"Pid Serveur 2 : %d\n",tab->pidServeur2);
  fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
  fprintf(stderr,"Pid Admin     : %d\n",tab->pidAdmin);
  for (int i=0 ; i<6 ; i++)
    fprintf(stderr,"%6d -%20s- %6d %6d %6d %6d %6d - %6d\n",tab->connexions[i].pidFenetre,
                                                      tab->connexions[i].nom,
                                                      tab->connexions[i].autres[0],
                                                      tab->connexions[i].autres[1],
                                                      tab->connexions[i].autres[2],
                                                      tab->connexions[i].autres[3],
                                                      tab->connexions[i].autres[4],
                                                      tab->connexions[i].pidModification);
  fprintf(stderr,"\n");
}

void HandlerSIGINT(int sig)
{
  if (idFilsPub > 0)
  {
    kill(idFilsPub, SIGTERM);
    waitpid(idFilsPub, NULL, 0);
    idFilsPub = -1;
  }

  if (fdPipe[0] >= 0)
  {
    close(fdPipe[0]);
    fdPipe[0] = -1;
    fprintf(stderr,"(SERVEUR) Pipe sortie fermé\n");
  }

  if (fdPipe[1] >= 0)
  {
    close(fdPipe[1]);
    fdPipe[1] = -1;
    fprintf(stderr,"(SERVEUR) Pipe entrée fermé\n");
  }

  msgctl(idQ, IPC_RMID, NULL);
  shmctl(idShm, IPC_RMID, NULL);
  semctl(idSem, 0, IPC_RMID);

  fprintf(stderr,"(SERVEUR) IPC supprimés\n");
  exit(0);
}
void HandlerSIGCHLD(int sig)
{
  int status, pid, i = 0;
  pid = wait(&status);
  // while (i < 6 && tab->connexions[i].pidModification != pid)
  //   i++;
  // tab->connexions[i].pidModification = 0;
  // siglongjmp(jumpBuffer, 1);
}