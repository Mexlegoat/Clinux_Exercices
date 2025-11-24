#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <mysql/mysql.h>
#include "protocole.h"   // contient tes #define et struct MESSAGE
#define ACCESBD 1000
MYSQL *connexion;
MYSQL_RES *resultat;
MYSQL_ROW tuple;

int idQ;

// ---------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------
int main()
{
    MESSAGE m, reponse;
    char requete[300];

    // Connexion MySQL
    connexion = mysql_init(NULL);
    if (connexion == NULL)
    {
        fprintf(stderr, "Erreur mysql_init\n");
        exit(1);
    }

    if (mysql_real_connect(connexion, "localhost", "root", "root", 
                           "BD2024", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Erreur connexion MySQL : %s\n", mysql_error(connexion));
        exit(1);
    }

    // File de messages
    idQ = msgget((key_t)CLE, 0600);
    if (idQ == -1)
    {
        perror("msgget");
        exit(1);
    }

    fprintf(stderr, "(AccesBD %d) Démarré.\n", getpid());

    // Boucle infinie
    while (1)
    {
        // Réception requête
        if (msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), ACCESBD, 0) == -1)
        {
            perror("msgrcv");
            exit(1);
        }

        fprintf(stderr, "(AccesBD %d) Message reçu (requete=%d, expediteur=%d)\n",
                getpid(), m.requete, m.expediteur);

        memset(&reponse, 0, sizeof(MESSAGE));
        reponse.type = m.expediteur;
        reponse.expediteur = getpid();
        reponse.requete = m.requete;

        // -------------------------------------------------------
        // REQUETE : CONSULT
        // -------------------------------------------------------
        switch(m.requete)
        {

          case CONSULT:
          fprintf(stderr, "(AccesBD) CONSULT nom = %s\n", m.data1);

          // Requête SQL : chercher par nom
          sprintf(requete,
                  "SELECT nom, gsm, email FROM USERS WHERE nom = '%s';",
                  m.data1);

          if (mysql_query(connexion, requete))
          {
              fprintf(stderr, "Erreur SQL : %s\n", mysql_error(connexion));
              strcpy(reponse.data1, "");    // vide si erreur
              reponse.data2[0] = '\0';
              reponse.texte[0] = '\0';
              break;
          }
          else
          {
              resultat = mysql_store_result(connexion);
              tuple = mysql_fetch_row(resultat);

              if (tuple == NULL)
              {
                  // Aucun utilisateur
                  strcpy(reponse.data1, "---");
                  strcpy(reponse.data2, "---");
                  strcpy(reponse.texte, "---");
              }
              else
              {
                  // tuple[0] = id
                  // tuple[1] = nom
                  // tuple[2] = gsm
                  // tuple[3] = email
                  strcpy(reponse.data1, tuple[1]); // nom
                  strcpy(reponse.data2, tuple[2]); // gsm
                  strcpy(reponse.texte, tuple[3]); // email
              }
              mysql_free_result(resultat);

                  if (msgsnd(idQ, &reponse, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      perror("msgsnd");

                  break;

              case MODIF2:
                  fprintf(stderr, "(AccesBD) MODIF2 id = %s\n", m.data1);

                  int idmod = atoi(m.data1);

                  sprintf(requete,
                          "UPDATE USERS SET gsm='%s', email='%s' WHERE id=%d;",
                          m.data2, m.texte, idmod);

                  if (mysql_query(connexion, requete))
                      strcpy(reponse.data1, "KO");
                  else
                      strcpy(reponse.data1, "OK");

                  if (msgsnd(idQ, &reponse, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      perror("msgsnd");

                  break;
          }
        }
    }

    mysql_close(connexion);
    return 0;
}
