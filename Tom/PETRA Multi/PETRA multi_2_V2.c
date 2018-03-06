/***********************************************************
 * PETRA MULTIPIECE
 * Circulation multiprocessus et multipieces sur le PETRA
 **********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <termios.h>

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
void *fctPiece(void *);
void *fctMauvaise(void *);
void HandlerSIGUSR1(int);
void HandlerSIGUSR2(int);
void HandlerSIGALRM(int);

int fd_petra_in, fd_petra_out;
int rc, mutexOwner = -1, condOwner = -1, timeoutOwner = -1;
short nbPieces = 0;
bool firstRun = true;
bool enMauvaise = false;

//Thread ids
pthread_t tid1, tid2, tid3, tidMauvaise;

//Mutex
pthread_mutex_t mutexChariot = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCondMauvaise = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCondMauvaise_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexMauvaise = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTimeout = PTHREAD_MUTEX_INITIALIZER;

//Variables de condition
pthread_cond_t condMauvaise = PTHREAD_COND_INITIALIZER;
pthread_cond_t condMauvaise_2 = PTHREAD_COND_INITIALIZER;

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

	//Creation threads piece (un thread par piece) et thread mauvaise
	pthread_create(&tid1, NULL, fctPiece, NULL);
	pthread_create(&tid2, NULL, fctPiece, NULL);
	pthread_create(&tid3, NULL, fctPiece, NULL);
	pthread_create(&tidMauvaise, NULL, fctMauvaise, NULL);

    pthread_exit(0);
}

void tempo(time_t sec, unsigned long nano)
{
	//Initialisation d'une instance de struct timespec a sec secondes et nano nanosecondes
	tim.tv_sec = sec;
	tim.tv_nsec = nano;

	nanosleep(&tim, NULL);
}

//Thread piece
void *fctPiece(void *p)
{	
	bool m = false;
	timer_t timerId;
	struct itimerspec timerOut;
	
	timer_create(CLOCK_REALTIME, 0, &timerId);
	
	//Armement du signal SIGUSR1 pour chaque thread piece
	struct sigaction act;
	act.sa_handler = HandlerSIGUSR1;
	act.sa_flags = 0;

	sigaction(SIGUSR1, &act, 0);
	
	//Armement du signal SIGUSR2 pour le thread mauvaise
	act.sa_handler = HandlerSIGUSR2;
	
	sigaction(SIGUSR2, &act, 0);
	
	//Armement du signal SIGALRM pour chaque thread piece
	act.sa_handler = HandlerSIGALRM;

	sigaction(SIGALRM, &act, 0);

	while(1)
	{
		read(fd_petra_in, &u_capt.byte, 1);
		if(!u_capt.capt.DE)
		{
			//printf(CLEAR_ECR);
			nbPieces++;
			
			pthread_mutex_lock(&mutexChariot);
			
			read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.capt.DE)
			{
				pthread_mutex_unlock(&mutexChariot);
				while(u_capt.capt.DE)
					read(fd_petra_in, &u_capt.byte, 1);
				pthread_mutex_lock(&mutexChariot);
				read(fd_petra_in, &u_capt.byte, 1);
			}
			
			mutexOwner = pthread_self();
			printf("Nouvelle piece\n");

			if(!firstRun)
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
			tempo(1, halfsec);

			//Activation Ventouse
			u_act.act.PV = 1;
			write(fd_petra_out, &u_act.byte, 1);
			tempo(1, 0);

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
			tempo(0, halfsec);

			firstRun = false;
			pthread_mutex_unlock(&mutexChariot);
			mutexOwner = -1;

			//Tant que la section critique mauvaise piece est en cours, les convoyeurs sont a l'arret
			//Dans ce cas, on attend qu'elle se termine et ainsi que les convoyeurs aient redemarre
			while(pthread_mutex_trylock(&mutexMauvaise) != 0);
			pthread_mutex_unlock(&mutexMauvaise);

			//Init timer a 8s
			timerOut.it_value.tv_sec = 8;
			timerOut.it_value.tv_nsec = 0;
			timerOut.it_interval.tv_sec = 0;
			timerOut.it_interval.tv_nsec = 0;

			timer_settime(timerId, 0, &timerOut, 0);
			
			//Attente d'une piece au niveau du slot
			timeoutOwner = pthread_self();
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.S)
				read(fd_petra_in, &u_capt.byte, 1);

			timeoutOwner = -1;
			
			//Arret timer
			timerOut.it_value.tv_sec = 0;
			timerOut.it_value.tv_nsec = 0;
			timerOut.it_interval.tv_sec = 0;
			timerOut.it_interval.tv_nsec = 0;

			timer_settime(timerId, 0, &timerOut, 0);

			//Attente trou/fin piece
			read(fd_petra_in, &u_capt.byte, 1);
			while(u_capt.capt.S)
				read(fd_petra_in, &u_capt.byte, 1);

			tempo(0, halfsec);
			
			//Si apres moins 0,5sec S passe ра1 -> Slot OK
			read(fd_petra_in, &u_capt.byte, 1);
			if(!u_capt.capt.S)
			{
				m = true;
				printf("Pas de Slot\n");
			}

			if(m == false)
				tempo(1, 600000000);
			else
				tempo(0, halfsec);

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
			tempo(0, halfsec);

			//Desactivation Grappin
			u_act.act.GA = 0;
			write(fd_petra_out, &u_act.byte, 1);

			tempo(1, 0);
			
			//Desactivation Arbre apres 1 seconde
			u_act.act.AA = 0;
			write(fd_petra_out, &u_act.byte, 1);

			//Init timer a 6s
			timerOut.it_value.tv_sec = 6;
			timerOut.it_value.tv_nsec = 0;
			timerOut.it_interval.tv_sec = 0;
			timerOut.it_interval.tv_nsec = 0;

			timer_settime(timerId, 0, &timerOut, 0);
			
			//Attente Corner
			timeoutOwner = pthread_self();
			read(fd_petra_in, &u_capt.byte, 1);
			while(!u_capt.capt.L2)
				read(fd_petra_in, &u_capt.byte, 1);

			timeoutOwner = -1;
			
			//Arret timer
			timerOut.it_value.tv_sec = 0;
			timerOut.it_value.tv_nsec = 0;
			timerOut.it_interval.tv_sec = 0;
			timerOut.it_interval.tv_nsec = 0;

			timer_settime(timerId, 0, &timerOut, 0);
			
			//Si L2 passe р 1 et L1 р 0 -> Corner OK
			if(u_capt.capt.L1)
			{
				m = true;
				printf("Petit Corner\n");
			}

			if(m)
			{
				condOwner = pthread_self();
				
				enMauvaise = true;
				pthread_mutex_lock(&mutexCondMauvaise);
				pthread_cond_signal(&condMauvaise);
				pthread_mutex_unlock(&mutexCondMauvaise);
				
				pthread_mutex_lock(&mutexCondMauvaise_2);
				while(enMauvaise)
					pthread_cond_wait(&condMauvaise_2, &mutexCondMauvaise_2);
				pthread_mutex_unlock(&mutexCondMauvaise_2);
				
				m = false;
				condOwner = -1;
				nbPieces--;
			}
			else
			{
				printf("Bonne piece\n");
				if(nbPieces != 0)
					nbPieces--;
				if(nbPieces == 0)
					tempo(3, 200000000);
			}
			
		}
		else
		{
		//	printf(CLEAR_ECR);
		//	printf("Plus de piece\n");
			
			if(nbPieces == 0)
			{
				firstRun = true;
				u_act.act.CP = 0b00;
				u_act.act.C1 = 0;
				u_act.act.C2 = 0;
				write(fd_petra_out, &u_act.byte, 1);
			}
		}
	}
	
	pthread_exit(0);
}

//Thread mauvaise
void *fctMauvaise(void *p)
{
	//Armement du signal SIGUSR2 pour le thread mauvaise
	struct sigaction act;
	act.sa_handler = HandlerSIGUSR2;
	act.sa_flags = 0;
	
	sigaction(SIGUSR2, &act, 0);
	
	while(1)
	{
		u_act.act.C1 = 1;
		if(!firstRun)
			u_act.act.C2 = 1;
		write(fd_petra_out, &u_act.byte, 1);

		pthread_mutex_lock(&mutexCondMauvaise);
		while(!enMauvaise)
			pthread_cond_wait(&condMauvaise, &mutexCondMauvaise);
		pthread_mutex_unlock(&mutexCondMauvaise);

		if(enMauvaise == false) printf("Pourquoi tu fais ca\n");
		pthread_mutex_lock(&mutexMauvaise);

		printf("Mauvaise piece\n");
		tempo(2, 300000000);

		//Desactivation Convoyeurs
		u_act.act.C1 = 0;
		u_act.act.C2 = 0;
		write(fd_petra_out, &u_act.byte, 1);

		if(mutexOwner != -1)
		{
			//On suspend le thread qui n'est pas mutexOwner ni condOwner
			if(mutexOwner != tid1 && condOwner != tid1) pthread_kill(tid1, SIGUSR1);
			if(mutexOwner != tid2 && condOwner != tid2) pthread_kill(tid2, SIGUSR1);
			if(mutexOwner != tid3 && condOwner != tid3) pthread_kill(tid3, SIGUSR1);
		}
		else
		{
			//On suspend les threads qui ne sont pas condOwner
			if(condOwner != tid1) pthread_kill(tid1, SIGUSR1);
			if(condOwner != tid2) pthread_kill(tid2, SIGUSR1);
			if(condOwner != tid3) pthread_kill(tid3, SIGUSR1);
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
		tempo(1, halfsec);

		//Activation Ventouse
		u_act.act.PV = 1;
		write(fd_petra_out, &u_act.byte, 1);
		tempo(1, 0);

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
		tempo(1, halfsec);

		//Desactivation Ventouse
		u_act.act.PV = 0;
		write(fd_petra_out, &u_act.byte, 1);

		//Plongeur remonte
		u_act.act.PA = 0;
		write(fd_petra_out, &u_act.byte, 1);
		read(fd_petra_in, &u_capt.byte, 1);
		while(u_capt.capt.PP)
			read(fd_petra_in, &u_capt.byte, 1);

		enMauvaise = false;
		pthread_mutex_unlock(&mutexChariot);
		pthread_cond_signal(&condMauvaise_2);
		pthread_mutex_unlock(&mutexMauvaise);
	}
	
	pthread_exit(0);
}

//Handler execute par un thread piece a la reception d'un SIGUSR1
void HandlerSIGUSR1(int sig)
{
	//Tant que la section critique mauvaise piece est en cours, le thread qui a recu le signal reste dans le Handler.
	pthread_mutex_lock(&mutexMauvaise);
	pthread_mutex_unlock(&mutexMauvaise);
}

//Handler execute par un thread piece et le thread mauvaise a la reception d'un SIGUSR2
void HandlerSIGUSR2
(int sig)
{
	//Tant que la section critique timeout est en cours, le thread qui a recu le signal reste dans le Handler.
	pthread_mutex_lock(&mutexTimeout);
	pthread_mutex_unlock(&mutexTimeout);
}

//Handler execute par un thread piece a la reception d'un SIGALRM
void HandlerSIGALRM(int sig)
{
	char c;
	bool restart = false;
	
	pthread_mutex_lock(&mutexTimeout);
	
	//Desactivation Convoyeurs
	u_act.act.C1 = 0;
	u_act.act.C2 = 0;
	write(fd_petra_out, &u_act.byte, 1);
	
	if(timeoutOwner != tid1) pthread_kill(tid1, SIGUSR2);
	if(timeoutOwner != tid2) pthread_kill(tid2, SIGUSR2);
	if(timeoutOwner != tid3) pthread_kill(tid3, SIGUSR2);
	if(enMauvaise == true)
		pthread_kill(tidMauvaise, SIGUSR2);
	else
		pthread_mutex_unlock(&mutexMauvaise);
	
	printf("Reprendre l'execution : r\nQuitter l'application : q\n");
	while(!restart) 
	{
		c = getchar();
		
		if(c == 'q' || c == 'Q')
		{
			pthread_mutex_destroy(&mutexChariot);
			pthread_mutex_destroy(&mutexCondMauvaise);
			pthread_mutex_destroy(&mutexMauvaise);
			pthread_mutex_destroy(&mutexTimeout);
			pthread_cond_destroy(&condMauvaise);
			
			/*pthread_kill(tid1, SIGINT);
			pthread_kill(tid2, SIGINT);
			pthread_kill(tid3, SIGINT);
			pthread_kill(tidMauvaise, SIGINT);*/
			
			exit(0);
		}
		else
		{
			if(c == 'r' || c == 'R')
			{
				restart = true;
				printf(CLEAR_ECR);
				
				//Reactivation Convoyeurs
				u_act.act.C1 = 1;
				u_act.act.C2 = 1;
				write(fd_petra_out, &u_act.byte, 1);
				
				pthread_mutex_unlock(&mutexTimeout);
			}
		}
	}
}