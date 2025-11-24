#include "mainwindowex3.h"
#include "ui_mainwindowex3.h"

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
using namespace std;

MainWindowEx3::MainWindowEx3(QWidget *parent):QMainWindow(parent),ui(new Ui::MainWindowEx3)
{
    ui->setupUi(this);
}

MainWindowEx3::~MainWindowEx3()
{
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles : ne pas modifier /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowEx3::setGroupe1(const char* Text)
{
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditGroupe1->clear();
    return;
  }
  ui->lineEditGroupe1->setText(Text);
}

void MainWindowEx3::setGroupe2(const char* Text)
{
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditGroupe2->clear();
    return;
  }
  ui->lineEditGroupe2->setText(Text);
}

void MainWindowEx3::setGroupe3(const char* Text)
{
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditGroupe3->clear();
    return;
  }
  ui->lineEditGroupe3->setText(Text);
}

void MainWindowEx3::setResultat1(int nb)
{
  char Text[20];
  sprintf(Text,"%d",nb);
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditResultat1->clear();
    return;
  }
  ui->lineEditResultat1->setText(Text);
}

void MainWindowEx3::setResultat2(int nb)
{
  char Text[20];
  sprintf(Text,"%d",nb);
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditResultat2->clear();
    return;
  }
  ui->lineEditResultat2->setText(Text);
}

void MainWindowEx3::setResultat3(int nb)
{
  char Text[20];
  sprintf(Text,"%d",nb);
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditResultat3->clear();
    return;
  }
  ui->lineEditResultat3->setText(Text);
}

bool MainWindowEx3::recherche1Selectionnee()
{
  return ui->checkBoxRecherche1->isChecked();
}

bool MainWindowEx3::recherche2Selectionnee()
{
  return ui->checkBoxRecherche2->isChecked();
}

bool MainWindowEx3::recherche3Selectionnee()
{
  return ui->checkBoxRecherche3->isChecked();
}

const char* MainWindowEx3::getGroupe1()
{
  if (ui->lineEditGroupe1->text().size())
  { 
    strcpy(groupe1,ui->lineEditGroupe1->text().toStdString().c_str());
    return groupe1;
  }
  return NULL;
}

const char* MainWindowEx3::getGroupe2()
{
  if (ui->lineEditGroupe2->text().size())
  { 
    strcpy(groupe2,ui->lineEditGroupe2->text().toStdString().c_str());
    return groupe2;
  }
  return NULL;
}

const char* MainWindowEx3::getGroupe3()
{
  if (ui->lineEditGroupe3->text().size())
  { 
    strcpy(groupe3,ui->lineEditGroupe3->text().toStdString().c_str());
    return groupe3;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les boutons ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowEx3::on_pushButtonLancerRecherche_clicked()
{
  int fTrace = open("Trace.log", O_WRONLY | O_CREAT | O_APPEND, 0777);
  if (fTrace == -1)
  {
    perror("Erreur lors de l'ouverture du fichier");
    exit(0);
  }
  if (dup2(fTrace, STDERR_FILENO) == -1)
  {
    perror("Erreur lors de la redirection de stderr");
    exit(0);
  }
  fprintf(stderr,"Clic sur le bouton Lancer Recherche\n");

  // TO DO
  int idFils1,idFils2,idFils3;
  int status;
  int cpt = 0, x;
  if (recherche1Selectionnee())
  {
    idFils1 = fork();
    cout << "creation du premier fils" << endl;
    if (idFils1 == 0)
    {
      cout << "Voici le PID du fils 1: " << getpid() << endl
           << "Voici le PPID du pere: " << getppid() << endl;
      execl("./Lecture", "Lecture", getGroupe1(), NULL);
      exit(1);
    }
    cpt++;
  }
  if (recherche2Selectionnee())
  {
    idFils2 = fork();
    cout << "creation du deuxieme fils"<< endl;
    if (idFils2 == 0)
    {
      cout << "Voici le PID du fils 2: " << getpid() << endl
           << "Voici le PPID du pere: " << getppid() << endl;
      execl("./Lecture", "Lecture", getGroupe2(), NULL);
      exit(1);
    }
    cpt++;
  }
  if (recherche3Selectionnee())
  {
    idFils3 = fork();
    cout << "creation du troisième fils"<< endl;
    if (idFils3 == 0)
    {
      cout << "Voici le PID du fils 3: " << getpid() << endl
           << "Voici le PPID du pere: " << getppid() << endl;
      execl("./Lecture", "Lecture", getGroupe3(), NULL);
      exit(1);
    }
    cpt++;
  }
  for (int i = 0; i < cpt; i++)
  {
    x = wait(&status);
    cout << i << " Fils mort.s" << endl;
    if (WIFEXITED(status))
    {
      if(x == idFils1)
      {
        setResultat1(WEXITSTATUS(status));
      }
      if(x == idFils2)
      {
        setResultat2(WEXITSTATUS(status));
      }
      if(x == idFils3)
      {
        setResultat3(WEXITSTATUS(status));
      }
    }

  }
  ::close(fTrace);
}

void MainWindowEx3::on_pushButtonVider_clicked()
{
  fprintf(stderr,"Clic sur le bouton Vider\n");
  ui->lineEditResultat1->clear();         //Fonction utilisé pour clear le champ Resultat1,2,3
  ui->lineEditResultat2->clear();  
  ui->lineEditResultat3->clear();
  setGroupe1("");
  setGroupe2("");
  setGroupe3("");
}

void MainWindowEx3::on_pushButtonQuitter_clicked()
{
  fprintf(stderr, "Clic sur le bouton Quitter\n");
  exit(0);
}
