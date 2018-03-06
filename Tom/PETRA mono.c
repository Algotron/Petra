/***********************************************************
 * PETRA MONOPIECE
 * Circulation monopiece sur le PETRA
 **********************************************************/

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>

#define halfsec 500000000
#define CLEAR_ECR "\33[2J"

struct timespec tim;

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
	unsigned T  : 1; /* cable H */
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

void tempo(time_t, unsigned long);

short mauvaise = 0;

int main(int argc, char *argv[])
{
	int fd_petra_in, fd_petra_out;
	int rc, end = 0;

	printf(CLEAR_ECR);

	//Ouverture fichier actuateursPETRA en ecriture simple
	if((fd_petra_out = open("/dev/actuateursPETRA", O_WRONLY)) == -1)
		perror("MAIN : Erreur ouverture PETRA_OUT");
	else
		printf("MAIN : PETRA_OUT opened\n");

	//Ouverture fichier capteursPETRA en lecture simple
	if((fd_petra_in = open("/dev/capteursPETRA", O_RDONLY)) == -1)
		perror("MAIN : Erreur ouverture PETRA_IN");
	else
		printf("MAIN : PETRA_IN opened\n");

	//Initialisation des actuateurs
	u_act.byte = 0x00;
	rc = write(fd_petra_out, &u_act.byte, 1);
	if(rc == -1)
        perror("MAIN : Erreur de write");

    while(1)
    {
    	read(fd_petra_in, &u_capt.byte, 1);
    	//Tant que piece dans reservoir
	    while(!u_capt.capt.DE)
	    {
	    	//Plongeur descend
	    	u_act.act.PA = 1;
	    	write(fd_petra_out, &u_act.byte, 1);
			tempo(3,0);

	    	//Activation Ventouse
	    	u_act.act.PV = 1;
	    	write(fd_petra_out, &u_act.byte, 1);
			tempo(2,0);

	    	//Plongeur remonte
	    	u_act.act.PA = 0;
	    	write(fd_petra_out, &u_act.byte, 1);
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.capt.PP != 0)
	    		read(fd_petra_in, &u_capt.byte, 1);

	    	//Chariot position 1
	    	u_act.act.CP = 0b01;
	    	write(fd_petra_out, &u_act.byte, 1);
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.capt.CS != 1)
	    		read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.capt.CS == 1)
	    		read(fd_petra_in, &u_capt.byte, 1);

			//Desactivation Ventouse
	    	u_act.act.PV = 0;
	    	write(fd_petra_out, &u_act.byte, 1);

	    	//Activation Convoyeur 1
	    	u_act.act.C1 = 1;
	    	write(fd_petra_out, &u_act.byte, 1);

	    	//Attente d'une piece
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.capt.S == 0)
				read(fd_petra_in, &u_capt.byte, 1);

	    	//Attente trou/fin piece
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(u_capt.capt.S == 1)
	    		read(fd_petra_in, &u_capt.byte, 1);

	    	tempo(0,halfsec);
	    	//Si apres moins 0,5sec S passe ра1 -> Slot OK
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	if (u_capt.capt.S == 0)
	    	{
	    		printf("Slot pas OK");
	    		mauvaise = 1;
	    	}
	    	else
	    	{
	    		printf("Slot OK\n");
	    	}

			//Attente Convoyeur 1
			tempo(3,halfsec);

			//Activation Grappin
	    	u_act.act.GA = 1;

	    	//Activation Arbre
	    	u_act.act.AA = 1;

	    	//Desactivation Convoyeur 1
	    	u_act.act.C1 = 0;

	    	//Activation Convoyeur 2
	    	u_act.act.C2 = 1;
	    	write(fd_petra_out, &u_act.byte, 1);

	    	//Attente Arbre
	    	read(fd_petra_in, &u_capt.byte, 1);
	    	while(!u_capt.capt.AP)
		    	read(fd_petra_in, &u_capt.byte, 1);

	    	//Stabilisation de la piece
			tempo(0,halfsec);

			//Desactivation Grappin
			u_act.act.GA = 0;
			write(fd_petra_out, &u_act.byte, 1);

			//Repositionnement Arbre quand piece arrive L1
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.L1)
				read(fd_petra_in, &u_capt.byte, 1);

			u_act.act.AA = 0;
			write(fd_petra_out, &u_act.byte, 1);

			//Attente Corner
			read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.capt.L1)
				read(fd_petra_in, &u_capt.byte, 1);

			//Si, quand L1 passe р 0, L2 passe р 1 -> Corner OK
			if(!u_capt.capt.L2)
			{
				mauvaise = 1;
				printf("Encoche pas OK\n");
			}
			else
			{
				printf("Encoche OK");
			}

			//Sequence Mauvaise piece
			if(mauvaise)
			{
				printf("Mauvaise piece");
				tempo(2,600000000);

				//Desactivation Convoyeur 2
	    		u_act.act.C2 = 0;

	    		//Chariot position 3
	    		u_act.act.CP = 0b11;
	    		write(fd_petra_out, &u_act.byte, 1);
	    		read(fd_petra_in, &u_capt.byte, 1);
	    		while(u_capt.capt.CS != 1)
	    			read(fd_petra_in, &u_capt.byte, 1);
	    		while(u_capt.capt.CS == 1)
	    			read(fd_petra_in, &u_capt.byte, 1);

		    	//Plongeur descend
		    	u_act.act.PA = 1;
		    	write(fd_petra_out, &u_act.byte, 1);
				tempo(1,halfsec);

		    	//Activation Ventouse
		    	u_act.act.PV = 1;
		    	write(fd_petra_out, &u_act.byte, 1);
				tempo(2,0);

		    	//Plongeur remonte
		    	u_act.act.PA = 0;
		    	write(fd_petra_out, &u_act.byte, 1);
		    	read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.capt.PP != 0)
		    		read(fd_petra_in, &u_capt.byte, 1);

		    	//Chariot position 2
		    	u_act.act.CP = 0b10;
		    	write(fd_petra_out, &u_act.byte, 1);
		    	read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.capt.CS != 1)
		    		read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.capt.CS == 1)
		    		read(fd_petra_in, &u_capt.byte, 1);

		    	//Plongeur descend
		    	u_act.act.PA = 1;
		    	write(fd_petra_out, &u_act.byte, 1);
				tempo(1,halfsec);

				//Desactivation Ventouse
		    	u_act.act.PV = 0;
		    	write(fd_petra_out, &u_act.byte, 1);

		    	//Plongeur remonte
		    	u_act.act.PA = 0;
		    	write(fd_petra_out, &u_act.byte, 1);
		    	read(fd_petra_in, &u_capt.byte, 1);
		    	while(u_capt.capt.PP != 0)
		    		read(fd_petra_in, &u_capt.byte, 1);
			}
			else
			{
				tempo(6,0);
				printf("Bonne piece");
			}

			//Chariot position 0
		    u_act.act.CP = 0b00;
		    write(fd_petra_out, &u_act.byte, 1);
		    read(fd_petra_in, &u_capt.byte, 1);
		    while(u_capt.capt.CS != 1)
		    	read(fd_petra_in, &u_capt.byte, 1);
		    while(u_capt.capt.CS == 1)
		    	read(fd_petra_in, &u_capt.byte, 1);

		    printf(CLEAR_ECR);
				
	    }

	    printf("Plus de pieces dans le reservoir!\n");
	}

    return 0;
}

void tempo(time_t sec, unsigned long nano)
{
	//Initialisation d'une instance de struct timespec ├а 0 secondes et 500000000 nanosecondes (soit 0,5 secondes)
	tim.tv_sec = sec;
	tim.tv_nsec = nano;

	nanosleep(&tim,NULL);
}
