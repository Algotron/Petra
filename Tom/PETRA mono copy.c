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
#include <signal.h>

#define halfsec 500000000

struct timespec tim;
struct sigaction Sig;

sigemptyset(sig.sa_mask);

sig.sa_handler = handler;
sig.sa_flags = 0;

sigaction(SIGALRM, &sig, NULL);

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
	unsigned H1 : 1;
	unsigned H2 : 1;
	unsigned H3 : 1;
	unsigned H4 : 1;
	unsigned H5 : 1;
	unsigned H6 : 1;
	unsigned    : 2;	
};

union
{
	struct CAPTEURS capt;
	unsigned char byte;
} u_capt;

void handler(int);
void tempo(time_t, long);


short mauvaise = 0;

int main(int argc, char *argv[])
{
	int fd_petra_in, fd_petra_out;
	int rc, end = 0;

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

    while(1)
    {
    	read(fd_petra_in, &u_capt.byte, 1);
	    while(!auact.act.DE)
	    {
	    	//Plongeur descend
	    	u_act.PA = 1;
	    	write(fd_petra_out, &u_act.byte, 1);
			tempo(1,halfsec);

	    	//Activation Ventouse
	    	u_act.PV = 1;
	    	write(fd_petra_out, &u_act.byte, 1);
			tempo(2,0);

	    	//Plongeur remonte
	    	u_act.PA = 0;
	    	write(fd_petra_out, &u_act.byte, 1);
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.PP != 0)
	    		read(fd_petra_in, &u_capt.byte, 1);


	    	//Chariot position 1
	    	uact.CP = 01;
	    	write(fd_petra_out, &u_act.byte, 1);
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.CS == 1)
	    		read(fd_petra_in, &u_capt.byte, 1);

			//Desactivation Ventouse
	    	u_act.PV = 0;
	    	write(fd_petra_out, &u_act.byte, 1);

	    	//Activation Convoyeur 1
	    	u_act.C1 = 1;
	    	write(fd_petra_out, &u_act.byte, 1);


	    	//Attente d'une piece
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.S == 0)
				read(fd_petra_in, &u_capt.byte, 1);

	    	//Attente trou/fin piece
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.S == 1)
	    		read(fd_petra_in, &u_capt.byte, 1);
	    	ALARM(2);

	    	//Si apres moins 1sec S passe à 1 -> Slot 
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	if (u_capt.S == 1)
	    		ALARM(0);

			//Attente convoyeur
			tempo(3,halfsec);

			//Activation Grapin
	    	u_act.GA = 1;

	    	//Activation Arbre
	    	u_act.AA = 1;

	    	//Desactivation Convoyeur 1
	    	u_act.C1 = 0;

	    	//Activation Convoyeur 1
	    	u_act.C2 = 1;

	    	write(fd_petra_out, &u_act.byte, 1);

	    	//Attente Arbre
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(!u_capt.AP)
		    	read(fd_petra_in, &u_capt.byte, 1);

	    	//Stabilisation de la piece
			tempo(0,halfsec);

			//Desactivation Grapin
			u_act.GA = 0;

			//Repositionnement Arbre quand piece arrive L1
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.L1)
				read(fd_petra_in, &u_capt.byte, 1);

				u_act.AA = 0;
				write(fd_petra_out, &u_act.byte, 1);

			read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.L1)
				read(fd_petra_in, &u_capt.byte, 1);

c			if(!u_capt.L2)
				mauvaise = 1;



			//Sequence Mauvaise piece
			if (mauvaise)
			{
				tempo(2,600000000);

				//Activation Convoyeur 1
	    		u_act.C2 = 0;
	    		u_act.CP = 11;
	    		write(fd_petra_out, &u_act.byte, 1);


		    	//Plongeur descend
		    	u_act.PA = 1;
		    	write(fd_petra_out, &u_act.byte, 1);
				tempo(1,halfsec);

		    	//Activation Ventouse
		    	u_act.PV = 1;
		    	write(fd_petra_out, &u_act.byte, 1);
				tempo(2,0);

		    	//Plongeur remonte
		    	u_act.PA = 0;
		    	write(fd_petra_out, &u_act.byte, 1);
		    	read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.PP != 0)
		    		read(fd_petra_in, &u_capt.byte, 1);


		    	//Chariot position 2
		    	uact.CP = 10;
		    	write(fd_petra_out, &u_act.byte, 1);
		    	read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.CS == 1)
		    		read(fd_petra_in, &u_capt.byte, 1);

		    	//Plongeur descend
		    	u_act.PA = 1;
		    	write(fd_petra_out, &u_act.byte, 1);
				tempo(1,halfsec);

				//Desactivation Ventouse
		    	u_act.PV = 0;
		    	write(fd_petra_out, &u_act.byte, 1);

		    	//Plongeur remonte
		    	u_act.PA = 0;
		    	write(fd_petra_out, &u_act.byte, 1);
		    	read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.PP != 0)
		    		read(fd_petra_in, &u_capt.byte, 1);


			}
			else
				tempo(6,0);

			//Chariot position 0
		    uact.CP = 00;
		    write(fd_petra_out, &u_act.byte, 1);
		    read(fd_petra_in, &u_capt.byte, 1);
		    while(u_capt.CS == 1)
		    	read(fd_petra_in, &u_capt.byte, 1);
				
	    }

	    printf("Plus de pièces dans le réservoir!\n");
	}

    return 0;
}


void handler(int sig)
{
	mauvaise = 1;
}

void tempo(time_t sec, long nano)
{
	//Initialisation d'une instance de struct timespec à 0 secondes et 500000000 nanosecondes (soit 0,5 secondes)
	tim.tv_sec = sec;
	tim.tv_nsec = nano;
}


