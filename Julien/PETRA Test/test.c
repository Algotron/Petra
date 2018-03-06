/***********************************************************
 * PETRA UTLITAIRE DE TEST
 * Activation d’actuateurs et lecture de capteurs du PETRA
 **********************************************************/

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>

#define CLEAR_ECR "\33[2J]"

struct ACTUATEURS
{
	unsigned CP : 2;
	unsigned C1 : 1;
	unsigned C2 : 1;
	unsigned PV : 1;
	unsigned PA : 1;
	unsigned AA : 1;
	unsigned GA : 1;
};

union
{
	struct ACTUATEURS act;
	unsigned char byte;
} u_act;

struct CAPTEURS
{
	unsigned L1 : 1;
	unsigned L2 : 1;
	unsigned T  : 1; /* cablé H */
	unsigned S  : 1;
	unsigned CS : 1;
	unsigned AP : 1;
	unsigned PP : 1;
	unsigned DE : 1;
/*	unsigned H1 : 1;
	unsigned H2 : 1;
	unsigned H3 : 1;
	unsigned H4 : 1;
	unsigned H5 : 1;
	unsigned H6 : 1;
	unsigned    : 2;	*/
};

union
{
	struct CAPTEURS capt;
	unsigned char byte;
} u_capt;

int main(int argc, char *argv[])
{
	int fd_petra_in, fd_petra_out;
	int rc,end=0;
	char buf;

	//Initialisation d'une instance de struct timespec à 0 secondes et 500000000 nanosecondes (soit 0,5 secondes)
	struct timespec tim;
	tim.tv_sec = 0;
	tim.tv_nsec = 500000000;

	//Ouverture fichier actuateursPETRA en écriture simple
	if ((fd_petra_out = open("/dev/actuateursPETRA", O_WRONLY)) == -1)
		perror("MAIN : Erreur ouverture PETRA_OUT");
	else
		printf ("MAIN : PETRA_OUT opened\n");

	//Ouverture fichier capteursPETRA en lecture simple
	if ((fd_petra_in = open("/dev/capteursPETRA", O_RDONLY)) == -1)
		perror ( "MAIN : Erreur ouverture PETRA_IN" );
	else
		printf ("MAIN : PETRA_IN opened\n");

	//Initialisation des actuateurs
	u_act.byte = 0x00;
	rc = write(fd_petra_out, &u_act.byte, 1);
	if (rc == -1)
        perror("MAIN : Erreur de write");

	do
	{
		do
		{
			printf(CLEAR_ECR);

			/*-----------------------------------------------
			 * Lecture et affichage de l'état des capteurs
			 *---------------------------------------------*/

			printf("CAPTEURS :\n");
			read(fd_petra_in, &u_capt.byte, 1);
			printf("DE  :\t\t\t%x\n",u_capt.capt.DE);
			printf("AP  :\t\t\t%x\n",u_capt.capt.AP);
			printf("CS  :\t\t\t%x\n",u_capt.capt.CS);
			printf("PP  :\t\t\t%x\n",u_capt.capt.PP);
			printf("T   :\t\t\t%x\n",u_capt.capt.T);
			printf("S   :\t\t\t%x\n",u_capt.capt.S);
			printf("L1  :\t\t\t%x\n",u_capt.capt.L1);
			printf("L2  :\t\t\t%x\n\n",u_capt.capt.L2);

			/*--------------------------------------------------
			 * Lecture et affichage de l'état des actuateurs
			 -------------------------------------------------*/

			printf("ACTUATEURS :\n");
			read(fd_petra_out, &u_act.byte, 1);
			printf("PV  : v\t\t\t%x\n",u_act.act.PV);
			printf("PA  : p\t\t\t%x\n",u_act.act.PA);
			printf("CPh : h\t\t\t%x\n",u_act.act.CP);
			printf("CPl : l\t\t\t%x\n",u_act.act.CP);
			printf("C1  : c\t\t\t%x\n",u_act.act.C1);
			printf("C2  : t\t\t\t%x\n",u_act.act.C2);
			printf("AA  : a\t\t\t%x\n",u_act.act.AA);
			printf("GA  : g\t\t\t%x\n\n",u_act.act.GA);

			printf("Quitter : q\n==>");

			//0,5 secondes d'attente
			nanosleep(&tim, NULL);
		}
		while(tcischars(0) <= 0);  //Boucle tant que aucun caractère en attente sur stdin

		read(1, &buf, 1);
		switch(buf)
		{
			case 'v' :
				if(u_act.act.PV == 1)
					u_act.act.PV = 0;
				else
					u_act.act.PV = 1;
				break;
			case 'p' :
				if(u_act.act.PA == 1)
					u_act.act.PA = 0;
				else
					u_act.act.PA = 1;
				break;
			//CPH
			case 'h' :
				if((u_act.act.CP || 01) == 1)
					u_act.act.CP &= 01;
				else
					u_act.act.CP &= 11;
				break;
			//CPL
			case 'l' :
				if((u_act.act.CP || 10) == 1)
					u_act.act.CP &= 10;
				else
					u_act.act.CP &= 11;
				break;
			case 'c' :
				if(u_act.act.C1 == 1)
					u_act.act.C1 = 0;
				else
					u_act.act.C1 = 1;
				break;
			case 't' :
				if(u_act.act.C2 == 1)
					u_act.act.C2 = 0;
				else
					u_act.act.C2 = 1;
				break;
			case 'a' :
				if(u_act.act.AA == 1)
					u_act.act.AA = 0;
				else
					u_act.act.AA = 1;
				break;
			case 'g' :
				if(u_act.act.GA == 1)
					u_act.act.GA = 0;
				else
					u_act.act.GA = 1;
				break;
			case 'q' :
				end = 1;
				break;
		}

		/* Ecriture des actuateurs */

		rc = write(fd_petra_out, &u_act.byte, 1);
		if (rc == -1)
            perror("MAIN : Erreur de write");
	}
	while(!end);

	//Réinisialisation des actuateurs
	u_act.byte = 0x00;
	write(fd_petra_out, &u_act.byte, 1);
	//Fermeture des fichiers
	close(fd_petra_in);
	close(fd_petra_out);
	return 0;
}
