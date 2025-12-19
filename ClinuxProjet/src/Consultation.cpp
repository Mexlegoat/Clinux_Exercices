#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <mysql.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "protocole.h"

int idQ,idSem;

int main()
{
  // Recuperation de l'identifiant de la file de messages
  fprintf(stderr,"(CONSULTATION %d) Recuperation de l'id de la file de messages\n",getpid());
  idQ = msgget(CLE, 0);
  if (idQ == -1)
  {
    perror("(CONSULTATION) msgget");
    exit(1);
  }

  // Recuperation de l'identifiant du sémaphore
  idSem = semget(CLE, 1, 0);  
  if (idSem == -1)
  {
    perror("(CONSULTATION) semget");
    exit(1);
  }
  // Lecture de la requête CONSULT
  fprintf(stderr,"(CONSULTATION %d) Lecture requete CONSULT\n",getpid());
  MESSAGE m;

  if (msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), getpid(), 0) == -1)
  {
    perror("(CONSULTATION) msgrcv CONSULT");
    exit(1);
  }
  // Tentative de prise bloquante du semaphore 0
  fprintf(stderr,"(CONSULTATION %d) Prise bloquante du sémaphore 0\n",getpid());
  struct sembuf op;
  op.sem_num = 0;
  op.sem_op  = -1;  // prise bloquante
  op.sem_flg = 0;

  if (semop(idSem, &op, 1) == -1)
  {
      perror("(CONSULTATION) semop take");
      exit(1);
  }

  // Connexion à la base de donnée
  MYSQL *connexion = mysql_init(NULL);
  fprintf(stderr,"(CONSULTATION %d) Connexion à la BD\n",getpid());
  if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(CONSULTATION) Erreur de connexion à la base de données...\n");
    fprintf(stderr,"(CONSULTATION) mysql_error: %s\n", mysql_error(connexion));
    exit(1);  
  }

  // Recherche des infos dans la base de données
  fprintf(stderr,"(CONSULTATION %d) Consultation en BD (%s)\n",getpid(),m.data1);
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  char requete[200];
  sprintf(requete, "SELECT gsm, email FROM UNIX_FINAL WHERE nom='%s'", m.data1);
  if(mysql_query(connexion,requete) != 0)
  {
    fprintf(stderr,"(CONSULTATION) mysql_error: %s\n", mysql_error(connexion));
    exit(1);
  }

  resultat = mysql_store_result(connexion);
  if(resultat == NULL)
  {
    fprintf(stderr,"(CONSULTATION) mysql_error: %s\n", mysql_error(connexion));
    exit(1);
  }
  // if ((tuple = mysql_fetch_row(resultat)) != NULL) ...
  if ((tuple = mysql_fetch_row(resultat)) == NULL)
  {
    strcpy(m.data1, "KO");
    strcpy(m.data2, "---");
    strcpy(m.texte, "---");
  }
  else
  {
      strcpy(m.data1, "OK");

      if (tuple[0] != NULL)
          strcpy(m.data2, tuple[0]);
      else
          strcpy(m.data2, "---");

      if (tuple[1] != NULL)
          strcpy(m.texte, tuple[1]);
      else
          strcpy(m.texte, "---");
  }

  m.type = m.expediteur;      // réponse vers client
  m.expediteur = getpid();
  m.requete = CONSULT;

  msgsnd(idQ, &m, sizeof(MESSAGE)-sizeof(long), 0);
  kill(m.type, SIGUSR1);

  if (resultat) mysql_free_result(resultat);
  mysql_close(connexion);
  // Libération du semaphore 0
  fprintf(stderr,"(CONSULTATION %d) Libération du sémaphore 0\n",getpid());
  op.sem_op = 1;
  if (semop(idSem, &op, 1) == -1)
  {
    perror("(CONSULTATION) semop release");
  }
  exit(0);
}