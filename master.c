//   John Citrowske
//   Project 3
//   3.17.22

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <time.h>
#include "config.h"

pid_t children[NUM_OF_PROCS];
int maxTime;
int slaves;
int shmAllocated;
int shmid;
char *programName;
int Processes;
int currentlyTerminating;
struct shmseg *shmp;
const int SHM_KEY = 777;
const int SHM_SIZE = 1024;
const int SHM_PERM = 0666;

struct shmseg {
	int source;
	int tickets[NUM_OF_PROCS];
	int choosing[NUM_OF_PROCS];
};


int isNum(char*);
char *getPerror();
int deallocateSharedMemory();
void endProgram(int, int);
void childTermHandler(int);
void ctrlCHandler(int);
void logTerm(char*);
static void timeoutHandler(int);
static int interruptsetup(void);
static int timersetup(void);
struct shmseg *shmp;



int main (int argc, char *argv[]) {
	signal(SIGINT, ctrlCHandler);
	signal(SIGCHLD, childTermHandler);
	programName = argv[0];
	maxTime = 100;
	slaves = 0;
	Processes = 0;
	currentlyTerminating=0;
	int opt;
	shmAllocated=0;
	int i;
	

   //***GETOPT***
while((opt = getopt(argc, argv, "hn:t:")) != -1){
	switch(opt){
			
	case'h':
	printf("Usage:\nchain [-h] [-n numOfProcs(slaves)] [-t ss maxTime] \nnumOfProcs(slaves) Number of processes \nmaxTime maximum time in seconds (default 100 seconds) after which the process should terminate itself if not completed. \n");
	return 0;
	break;

	case'n':
	slaves = atoi(optarg);
	if(slaves>NUM_OF_PROCS){
		printf("Slaves cannot be more than 20 \n");
		slaves = 0;
		exit(0);
	}
	break;
		      
	case't':
	maxTime = atoi(optarg);
      }
   }
	shmAllocated = 1;
	
	shmid = shmget(SHM_KEY, sizeof(struct shmseg), SHM_PERM|IPC_CREAT);
	if (shmid == -1) {
		char *output = getPerror();
		perror(output);
		return 1;
	}
	
	shmp = shmat(shmid, NULL, 0);
	if (shmp == (void *) -1) {
		char *output = getPerror();
		perror(output);
		return 1;
	}
	
	shmp->source = 0;
        for (i = 0; i < NUM_OF_PROCS; i++) {
                shmp->tickets[i] = 0;
                shmp->choosing[i] = 0;
        }
 	
	pid_t childpid = 0;
	for (i = 0; i < slaves; i++) {
		if ((childpid = fork()) == -1) {
			char *output = getPerror();
			perror(output);
			return 1;
		} else if (childpid == 0) {
			char strProcNum[10];
			sprintf(strProcNum, "%d", i);
			char strShmid[100];
			sprintf(strShmid, "%d", shmid);
			char *args[] = {"./slave", strProcNum, strShmid, (char*)0};
			execvp("./slave", args);
			
			char *output = getPerror();
			perror(output);
			printf("TEST from fork 2");
			return 1;
		} else {
			Processes++;
			children[i] = childpid;
		}
	}
	
	if (interruptsetup() == -1) {
		char *output = getPerror();
		perror(output);
		return 1;
	}
	if (timersetup() == -1) {
		char *output = getPerror();
		perror(output);
		return 1;
	}
	for( ; ; );
	
   return 0;
}
int isNum (char *str) {
	int i;
	for (i = 0; i < strlen(str); i++) {
		if (!isdigit(str[i])) return 0;
	}
	return 1;
}

char *getPerror () {
	char* output = strdup(programName);
	strcat(output, ": Error");
	return output;
}



static int timersetup(void) {
	struct itimerval value;
	value.it_interval.tv_sec = maxTime;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_PROF, &value, NULL));
}

static int interruptsetup(void) {
	struct sigaction act;
	act.sa_handler = timeoutHandler;
	act.sa_flags = 0;
	return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}


void ctrlCHandler(int s) {
	currentlyTerminating = 1;
	logTerm("ctrl+C");
	endProgram(1, 1);
}

static void timeoutHandler(int s) {
	currentlyTerminating = 1;
	printf("Timer ended. Now exiting program...\n");
	logTerm("timeout");
	endProgram(1, 1);
}


void childTermHandler(int s) {
	Processes--;
	if (Processes < 1 && !currentlyTerminating) {
		printf("All children have terminated. Now exiting program...\n");
		logTerm("all children terminated");
		endProgram(1, 0);
	}		
}


void logTerm(char *method) {
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	FILE *fptr = fopen("cstest", "a");
	if (fptr == NULL) {
		printf("Error: unable to open 'cstest' file.\n");
		exit(0);
	}
	fprintf(fptr, "%d:%d:%d Program ended. Termination method: %s\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, method);
}	

int deallocateSharedMemory() {
	int returnValue;
	printf("Deallocating shared memory...\n");
	returnValue = shmdt(shmp);
	if (returnValue == -1) {
		char *output = getPerror();
		perror(output);
		exit(1);
	}
	
	returnValue = shmctl(shmid, IPC_RMID, NULL);
	if (returnValue == -1) {
		char *output = getPerror();
		perror(output);
		exit(1);
	}

	return 0;
}

void endProgram (int s, int killChild) {
	if (killChild) {
		int i;
		for (i = 0; i < slaves; i++) {
			if ((kill(children[i], SIGKILL)) == -1) {
				char *output = getPerror();
				perror(output);
			}
		}
	}
	if (shmAllocated) deallocateSharedMemory();
	exit(0);
}
