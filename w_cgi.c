/*
 * w_cgi.c
 *
 *  Created on: Jul 6, 2010
 *      Author: zoli
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>

#include "wmrs200log.h"

int main(int argc, char *argv[])
{
	wmrs_t *w;
	int shmid;

	/* create shared memory for WMRS communication */
	if((shmid = shmget(1962, sizeof(wmrs_t), IPC_CREAT | 0666)) < 0){
		exit(EXIT_FAILURE);
	}
	if((w = shmat(shmid, NULL, SHM_RDONLY)) == (void *)-1){
		exit(EXIT_FAILURE);
	}

	puts("Content-Type: text/plain");
	puts("Access-Control-Allow-Origin: *\n");
	printf("%3.1f %d %3.1f %d %d %3.1f %3.1f %d %3.1f %3.1f %d %d %d %ld",
	/* index:      0             1           2             3       4        5        6        7 */
			w->s[0].temp, w->s[0].rh, w->s[1].temp, w->s[1].rh, w->relP, w->wind, w->gust, w->windDir,
	/* index:  8         9              10         11        12        13 */
			w->prec1, w->prec24, w->s[1].sBatt, w->wBatt, w->pBatt, w->timestamp);
	shmdt(w);
	exit(EXIT_SUCCESS);
}
/* SDG */
