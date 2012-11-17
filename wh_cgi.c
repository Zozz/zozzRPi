/*
 * wh_cgi.c - cgi program for historical data
 *
 *  Created on: Nov 26, 2011
 *      Author: zoli
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>

#include "wmrs200log.h"

int main(int argc, char *argv[])
{
	wmrs_t *w;
	int shmid, i;

	/* create shared memory for WMRS communication */
	if((shmid = shmget(1962, sizeof(wmrs_t), IPC_CREAT | 0666)) < 0){
		exit(EXIT_FAILURE);
	}
	if((w = shmat(shmid, NULL, SHM_RDONLY)) == (void *)-1){
		exit(EXIT_FAILURE);
	}

	puts("Content-Type: text/plain");
	puts("Access-Control-Allow-Origin: *\n");
	// Google Chart Basic Text Format
	for(i = 0; i < 24; i++){
		printf("%3.1f", w->tHist[i]);
		putchar(i < 23 ? ',' : '|');
	}
	for(i = 0; i < 24; i++){
		printf("%d", w->rhHist[i]);
		if(i < 23) putchar(',');
	}
	shmdt(w);
	exit(EXIT_SUCCESS);
}
/* SDG */
