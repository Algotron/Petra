/***********************************************************
 * PETRA MULTIPIECE
 * Circulation multiprocessus et multipieces sur le PETRA
 **********************************************************/

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

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
void *fctThread(void *);
void *fctMauvaise(void *);
void Handler(int);

int fd_petra_in, fd_petra_out;
int rc, mutexOwner = -1, condOwner = -1;
bool nfirstRun = true;
short attente = 0;

pthread_t tid1, tid2, tid3, tidm;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexChariot = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexMauvaise = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void main()
{
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

	//Creation threads sequence Petra (un thread par piece) et sequence mauvaise piece
	pthread_create(&tid1, NULL, fctThread, NULL);
	pthread_create(&tid2, NULL, fctThread, NULL);
	pthread_create(&tid3, NULL, fctThread, NULL);
	pthread_create(&tidm, NULL, fctMauvaise, NULL);

    pthread_exit(0);
}

void tempo(time_t sec, unsigned long nano)
{
	//Initialisation d'une instance de struct timespec a 0 secondes et 500000000 nanosecondes (soit 0,5 secondes)
	tim.tv_sec = sec;
	tim.tv_nsec = nano;

	nanosleep(&tim,NULL);
}

void *fctThread(void *p)
{	

	struct sigaction act;
	act.sa_handler = Handler;
	act.sa_flags = 0;

	sigaction(SIGUSR1, &act, 0);

	while(1)
	{
		bool m = false;

		read(fd_petra_in, &u_capt.byte, 1);
		if(!u_capt.capt.DE)
		{
			printf(CLEAR_ECR);

			pthread_mutex_lock(&mutexChariot);
			mutexOwner = pthread_self();
			printf("Nouvelle piece\n");

			if(!nfirstRun)
			{
				//Chariot position 0
				u_act.act.CP = 0b00;
				write(fd_petra_out, &u_act.byte, 1);
				read(fd_petra_in, &u_capt.byte, 1);
				while(!u_capt.capt.CS)
					read(fd_petra_in, &u_capt.byte, 1);
				while(u_capt.capt.CS)
					read(fd_petra_in, &u_capt.byte, 1);
			}

			//Plongeur descend
			u_act.act.PA = 1;
			write(fd_petra_out, &u_act.byte, 1);
			tempo(1,halfsec);

			//Activation Ventouse
			u_act.act.PV = 1;
			write(fd_petra_out, &u_act.byte, 1);
			tempo(1,0);

			//Plongeur remonte
			u_act.act.PA = 0;
			write(fd_petra_out, &u_act.byte, 1);
			read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.capt.PP)
				read(fd_petra_in, &u_capt.byte, 1);

			//Chariot position 1
			u_act.act.CP = 0b01;
			write(fd_petra_out, &u_act.byte, 1);
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.CS)
				read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.capt.CS)
				read(fd_petra_in, &u_capt.byte, 1);

			//Desactivation Ventouse
			u_act.act.PV = 0;
			write(fd_petra_out, &u_act.byte, 1);
			
			tempo(0,halfsec);

			nfirstRun = false;
			pthread_mutex_unlock(&mutexChariot);
			mutexOwner = -1;

			//Si mutexMauvaise pris pendant la section critique precedente, les convoyeurs auront ete arretes.
			//Dans ce cas, on relance la tempo de 2s une fois mutexMauvaise relache pour garder l'ecart de 2s entre chaque piece.
			if(pthread_mutex_trylock(&mutexMauvaise) != 0)
			{
				pthread_mutex_lock(&mutexMauvaise);
				pthread_mutex_unlock(&mutexMauvaise);
			}
			else
			{
				pthread_mutex_unlock(&mutexMauvaise);
			}

			//Attente d'une piece
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.S)
				read(fd_petra_in, &u_capt.byte, 1);

			//Attente trou/fin piece
			read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.capt.S)
				read(fd_petra_in, &u_capt.byte, 1);

			tempo(0,halfsec);
			//Si apres moins 0,5sec S passe ра1 -> Slot OK
			read(fd_petra_in, &u_capt.byte, 1);
			if(!u_capt.capt.S)
			{
				m = true;
				printf("Pas de Slot\n");
			}

			if(m == false)
				tempo(1,600000000);
			else
				tempo(0,halfsec);

			//Activation Grappin
			u_act.act.GA = 1;

			//Activation Arbre
			u_act.act.AA = 1;

			write(fd_petra_out, &u_act.byte, 1);

			//Attente Arbre
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.AP)
				read(fd_petra_in, &u_capt.byte, 1);

			if(pthread_mutex_trylock(&mutexMauvaise) == 0)
			{
				//Activation Convoyeur 2
				u_act.act.C2 = 1;
				write(fd_petra_out, &u_act.byte, 1);
				pthread_mutex_unlock(&mutexMauvaise);
			}
			
			//Stabilisation de la piece
			tempo(0,halfsec);

			//Desactivation Grappin
			u_act.act.GA = 0;
			write(fd_petra_out, &u_act.byte, 1);

			//Repositionnement Arbre
			tempo(1,0);
			
			//Desactivation Arbre
			u_act.act.AA = 0;
			write(fd_petra_out, &u_act.byte, 1);

			//Attente Corner
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.L2)
				read(fd_petra_in, &u_capt.byte, 1);
			//Si L2 passe р 1 et L1 р 0 -> Corner OK
			if(u_capt.capt.L1)
			{
				m = true;
				printf("Petit Corner\n");
			}

			if(m)
			{
				condOwner = pthread_self();
				pthread_mutex_lock(&mutex);
				pthread_cond_signal(&cond);
				pthread_mutex_unlock(&mutex);
				
				pthread_mutex_lock(&mutex);
				pthread_cond_wait(&cond, &mutex);
				pthread_mutex_unlock(&mutex);
				condOwner = -1;
			}
			else
				printf("Bonne piece");
		}
		else
		{
			printf(CLEAR_ECR);
			printf("Plus de piece\n");

			/*printf("Quitter : o/n\n");
			char c;
			c = getchar();

			if(c == 'o' || c == 'O')
				exit(0);*/
		}
	}
	
	pthread_exit(0);
}

void *fctMauvaise(void *p)
{
	printf("Dans thread mauvaise\n");
	while(1)
	{
		u_act.act.C1 = 1;
		if(!nfirstRun)
			u_act.act.C2 = 1;
		write(fd_petra_out, &u_act.byte, 1);

		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);
		pthread_mutex_unlock(&mutex);

		pthread_mutex_lock(&mutexMauvaise);

		printf("Mauvaise piece");
		tempo(2,300000000);

		//Desactivation Convoyeurs
		u_act.act.C1 = 0;
		u_act.act.C2 = 0;
		write(fd_petra_out, &u_act.byte, 1);

		if(mutexOwner != -1)
		{
			//On suspend le thread qui n'est pas mutexOwner
			if(mutexOwner != tid1) pthread_kill(tid1, SIGUSR1);
			if(mutexOwner != tid2) pthread_kill(tid2, SIGUSR1);
			if(mutexOwner != tid3) pthread_kill(tid3, SIGUSR1);
		}
		else
		{
			//On suspend les threads qui ne sont pas condOwner
			pthread_kill(tid1, SIGUSR1);
			pthread_kill(tid2, SIGUSR1);
			pthread_kill(tid3, SIGUSR1);
		}

		pthread_mutex_lock(&mutexChariot);

		//Chariot position 3
		u_act.act.CP = 0b11;
		write(fd_petra_out, &u_act.byte, 1);
		read(fd_petra_in, &u_capt.byte, 1);
		while(!u_capt.capt.CS)
			read(fd_petra_in, &u_capt.byte, 1);
		while(u_capt.capt.CS)
			read(fd_petra_in, &u_capt.byte, 1);

		//Plongeur descend
		u_act.act.PA = 1;
		write(fd_petra_out, &u_act.byte, 1);
		tempo(1,halfsec);

		//Activation Ventouse
		u_act.act.PV = 1;
		write(fd_petra_out, &u_act.byte, 1);
		tempo(1,0);

		//Plongeur remonte
		u_act.act.PA = 0;
		write(fd_petra_out, &u_act.byte, 1);
		read(fd_petra_in, &u_capt.byte, 1);
		while(u_capt.capt.PP)
			read(fd_petra_in, &u_capt.byte, 1);

		//Chariot position 2
		u_act.act.CP = 0b10;
		write(fd_petra_out, &u_act.byte, 1);
		read(fd_petra_in, &u_capt.byte, 1);
		while(!u_capt.capt.CS)
			read(fd_petra_in, &u_capt.byte, 1);
		while(u_capt.capt.CS)
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
		while(u_capt.capt.PP)
			read(fd_petra_in, &u_capt.byte, 1);

		pthread_mutex_unlock(&mutexChariot);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutexMauvaise);
	}
	
	pthread_exit(0);
}

void Handler(int sig)
{
	//Tant que la section critique mauvaise piece est en cours, le thread qui a recu le signal reste dans le handler.
	pthread_mutex_lock(&mutexMauvaise);
	pthread_mutex_unlock(&mutexMauvaise);
}
