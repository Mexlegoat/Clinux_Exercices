COMP=g++ -Wall -D DEBUG

Exercice2: FichierUtilisateur.o moc_mywindow.o mywindow.o main.o
	$(COMP) FichierUtilisateur.o moc_mywindow.o mywindow.o main.o -o Exercice2 -lQt5Widgets -lQt5Gui -lQt5Core

FichierUtilisateur.o:	FichierUtilisateur.cpp FichierUtilisateur.h
			$(COMP) FichierUtilisateur.cpp -c

moc_mywindow.o: moc_mywindow.cpp mywindow.h
		$(COMP) moc_mywindow.cpp -c

mywindow.o: mywindow.cpp mywindow.h ui_mywindow.h FichierUtilisateur.h
	$(COMP) mywindow.cpp -c

main.o: main.cpp mywindow.h
	$(COMP) main.cpp -c


