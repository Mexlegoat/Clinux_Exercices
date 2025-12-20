#include "FichierUtilisateur.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
int estPresent(const char* nom)
{
  UTILISATEUR user;
  int fd = open(FICHIER_UTILISATEURS, O_RDONLY);


  if (fd == -1)
  {
    perror("open");
    return -1;
  }
  lseek(fd, 0, SEEK_SET);
  int pos = 1;
  while (read(fd, &user, sizeof(UTILISATEUR)) == sizeof(UTILISATEUR)) {
    if (strcmp(user.nom, nom) == 0) {
        ::close(fd);
        return pos;
    }
      pos++;
  }
  if (::close(fd) == -1) perror("close");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////
int hash(const char* motDePasse)
{
  int i = 1, somme = 0;
  while(*motDePasse != '\0')
  {
    int code = *motDePasse;
    somme += i * code;
    i++;
    motDePasse++;
  }
  return somme % 97;
}

////////////////////////////////////////////////////////////////////////////////////
void ajouteUtilisateur(const char* nom, const char* motDePasse)
{
  int fd = open(FICHIER_UTILISATEURS, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
  UTILISATEUR user;
  strcpy(user.nom, nom);
  int code = ::hash(motDePasse);
  user.hash = code;
  if (write(fd, &user, sizeof(user)) == -1) perror("Erreur d'ecriture de l'utilisateur");
  if(::close(fd) == -1) perror("close");
}

////////////////////////////////////////////////////////////////////////////////////
int verifieMotDePasse(int pos, const char* motDePasse)
{
  int fd = open(FICHIER_UTILISATEURS, O_RDONLY);
  int code = ::hash(motDePasse);
  UTILISATEUR user;

  if (fd == -1)
  {
    perror("erreur d'ouverture du fichier");
    if(::close(fd) == -1) perror("close");
    return -1;
  }
  if (lseek(fd, (pos - 1) * sizeof(UTILISATEUR), SEEK_SET) == -1)
  {
    perror("Erreur de recherche");
    if(::close(fd) == -1) perror("close");
    return -1;
  }
  if(read(fd, &user, sizeof(UTILISATEUR)) != sizeof(UTILISATEUR))
  {
    perror("Erreur de lecture");
    if(::close(fd) == -1) perror("close");
    return -1;
  }
  if(::close(fd) == -1) perror("close");
  if (user.hash == code)
  {
    return 1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////
int listeUtilisateurs(UTILISATEUR *vecteur) // le vecteur doit etre suffisamment grand
{
  int cpt= 0;
  int fd = open(FICHIER_UTILISATEURS, O_RDONLY);

  if (fd == -1)
  {
    ::close(fd);
    return -1;
  }
  while (read(fd, &vecteur[cpt], sizeof(UTILISATEUR)) == sizeof(UTILISATEUR))
  {
    cpt++;
  }

  return cpt;
}
void supprimerUtilisateur(const char* nom)
{
  int fd = open(FICHIER_UTILISATEURS, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return;
    }

    int fdTmp = open("tmp.dat", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fdTmp == -1)
    {
        perror("open tmp");
        close(fd);
        return;
    }

    UTILISATEUR user;
    int trouve = 0;

    while (read(fd, &user, sizeof(UTILISATEUR)) == sizeof(UTILISATEUR))
    {
        if (strcmp(user.nom, nom) == 0)
        {
            trouve = 1;
            continue; // ne pas écrire cet utilisateur (on saute le write)
        }
        write(fdTmp, &user, sizeof(UTILISATEUR));
    }

    close(fd);
    close(fdTmp);

    if (!trouve)
    {
        unlink("tmp.dat");       // rien à supprimer
        return;
    }

    // Remplacer l'ancien fichier par le nouveau
    unlink(FICHIER_UTILISATEURS);
    rename("tmp.dat", FICHIER_UTILISATEURS);
}
