#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "config.h"

void lock(int);
void use_resource(int);
void unlock(int);
char *getPerror(char*);
void logMessage(char*, int, char*);


struct shmseg {
	int source;
	int tickets[NUM_OF_PROCS];
	int choosing[NUM_OF_PROCS];
};
struct shmseg *shmp;


int main (int argc, char *argv[]) {
	int procNum = atoi(argv[1]) + 1;
	int shmid = atoi(argv[2]);
	int semid = atoi(argv[3]);
	
	int randNumsLower = 1;
	int randNumsUpper = 5;
  	int randNums;
	srand(time(0) * procNum); 
	
	shmp = shmat(shmid, NULL, 0);
	if (shmp == (void *) -1) {
		char *output = getPerror(argv[0]);
		perror(output);
		exit(1);
	}
	
	char intToString[3];
	sprintf(intToString, "%d", procNum);
	char logfile[10] = "logfile.";
	strcat(logfile, intToString);
	
	int i = 0;
	for (i = 0; i < 5; i++) {
		struct sembuf sb = {0, -1, 0};
		logMessage("Request to enter critical section. process #: ", procNum, logfile);
		if (semop(semid, &sb, 1) == -1) { /* Lock */
			char *output = getPerror(argv[0]);
			perror(output);
			exit(1);
		}
		logMessage("Critical section entered. process #: ", procNum, logfile);
		randNums = (rand() % (randNumsUpper - randNumsLower + 1)) + randNumsLower;
		sleep(randNums);
		use_resource(procNum - 1);
		logMessage("Data entry in 'cstest' file. process #: ", procNum, logfile);
		randNums = (rand() % (randNumsUpper - randNumsLower + 1)) + randNumsLower; 
		sleep(randNums);
		logMessage("Critical section exited. process #: ", procNum, logfile);
		sb.sem_op = 1;
		if (semop(semid, &sb, 1) == -1) {
			char *output = getPerror(argv[0]);
			perror(output);
			exit(1);
		}
	}
	
	
        return 0;
}

void lock(int procNum) {
	shmp->choosing[procNum] = 1;
	MEMBAR; 
	
	int max_ticket = 0;

	int i;
	for (i = 0; i < NUM_OF_PROCS; ++i) {
		int ticket = shmp->tickets[i];
		max_ticket = ticket > max_ticket ? ticket : max_ticket;
	}
	
	shmp->tickets[procNum] = max_ticket + 1;
	
	MEMBAR;
	shmp->choosing[procNum] = 0;
	MEMBAR;
	
	int other;
	for (other = 0; other < NUM_OF_PROCS; ++other) {
		
		while (shmp->choosing[other]) {
		}
		
		MEMBAR;
		
		while (shmp->tickets[other] != 0 && (shmp->tickets[other] < shmp->tickets[procNum] || (shmp->tickets[other] == shmp->tickets[procNum] && other < procNum))) {
		}
		
	}
}

// EXIT
void unlock(int procNum) {
	MEMBAR;
	shmp->tickets[procNum] = 0;
}


//CRITICAL SECTION
void use_resource(int procNum) {
	if (shmp->source != 0) {
		printf("Resource was acquired by %d, but is still in-use by %d!\n", procNum, shmp->source);
	}
	shmp->source = procNum;
	int realProcNum = procNum + 1;
	printf("%d in use...\n", realProcNum);
	
	time_t rawtime;
	struct tm * timeinfo;
	FILE *fptr = fopen("cstest", "a");
	if (fptr == NULL) {
		printf("Error: cant open cstest file.\n");
		exit(0);
	}
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	fprintf(fptr, "%d:%d:%d Queue %d modified by process # %d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, shmp->tickets[procNum], realProcNum);
	fclose(fptr);
	
	MEMBAR;
	
	shmp->source = 0;
}

char *getPerror(char *programName) {
	char* output = strdup(programName);
	strcat(output, ": Error");
	return output;
}

//LOGGING MESSAGE FUNCTION
void logMessage(char *message, int procNum, char *fileName) {
	time_t rawtime;
        struct tm * timeinfo;
	time(&rawtime);
        timeinfo = localtime(&rawtime);
	FILE *fptr = fopen(fileName, "a");
        if (fptr == NULL) {
                printf("Error: cant open log file.\n");
                exit(0);
        }
        fprintf(fptr, "%d:%d:%d %s%d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, message, procNum);
        fclose(fptr);	
}
