/*
 * wmrs200log.c
 * This program communicates with Oregon WMRS200 (and probably WMR100) weather stations.
 * It is based on the very useful protocol description on http://github.com/ejeklint/WLoggerDaemon/blob/master/Station_protocol.md
 * and the Linux driver found at http://www.sdic.ch/innovation/contributions
 *
 * Copyright (C) Kovács Zoltán 2010 <0x4b5a@gmail.com>
 *
 * wmrs200log.c is free software.
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * wmrs200log.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wmrs200log.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <libusb-1.0/libusb.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <syslog.h>

#include "wmrs200log.h"

#define ever ;;
#define FOXBOARD
// record length definitions
#define DLEN 10
#define TLEN 10
#define WLEN 9
#define PLEN 6
#define RLEN 15

FILE *fp;
char disp[8][50];
char *winddir[] = {"N","NNE","NE","ENE", "E", "SEE", "SE", "SSE", "S", "SSW", "SW", "SWW", "W", "NWW", "NW", "NNW"};
wmrs_t *w;
time_t t;
libusb_device_handle *wmrs;

// used for debug purposes
void printBytes(unsigned char *bytes, int len)
{
  	int i;

	for(i = 0; i < len; i++)
		printf("%02x ", bytes[i]);
	printf("\n");
}

// frees resources on exit
void cleanup(int dummy)
{
    libusb_release_interface(wmrs, 0);
    libusb_close(wmrs);
	libusb_exit(NULL);
	fclose(fp);
	shmdt(w);
    exit(EXIT_SUCCESS);
}

void processRecord(unsigned char *rec)
{
	int flags, i, csum;

	csum = (flags = rec[0]) + rec[1];
	switch(rec[1]){	// record type
	case 0x60:	// date
		for(i = 2; i < DLEN; i++)
			csum += rec[i];
		if(csum != rec[DLEN]+rec[DLEN+1]*256)
			break;	// checksum error

		sprintf((char *)&disp[0],"*Power: %d, Batt: %d, RFsync: %d, RFsig: %d", (flags>>7)&1,
				(flags>>6)&1, (flags>>5)&1, (flags>>4)&1);
		sprintf((char *)&disp[1],"   Date: %02d/%02d/%02d %02d:%02d TZ%d", rec[8], rec[7], rec[6],
				rec[5], rec[4], rec[9]);
		break;
	case 0x42:	// temp/humidity sensors
		for(i = 2; i < TLEN; i++)
			csum += rec[i];
		if(csum != rec[TLEN]+rec[TLEN+1]*256)
			break;	// checksum error

		int sensor = rec[2] & 0x0F;

		if(rec[4] & 0x80)	// negative value
			w->s[sensor].temp = -((rec[4] & 0x7F)*256+rec[3]);
		else
			w->s[sensor].temp = rec[4]*256+rec[3];
		w->s[sensor].temp /= 10.0;

		w->s[sensor].rh = rec[5];

		if(rec[7] & 0x80)	// negative value
			w->s[sensor].dew = -((rec[7] & 0x7F)*256+rec[6]);
		else
			w->s[sensor].dew = rec[7]*256+rec[6];
		w->s[sensor].dew /= 10.0;

		w->s[sensor].sBatt = (flags >> 6) & 1;
		sprintf((char *)&disp[2+sensor],"*Sensor%d T: %3.1f Rh: %d%% Dew: %3.1f Batt: %d", sensor, w->s[sensor].temp,
				w->s[sensor].rh, w->s[sensor].dew, w->s[sensor].sBatt);

		w->timestamp = t;
		break;
	case 0x46:	// pressure
		for(i = 2; i < PLEN; i++)
			csum += rec[i];
		if(csum != rec[PLEN]+rec[PLEN+1]*256)
			break;	// checksum error

		w->absP = (rec[3]&0x0F)*256+rec[2];
		w->relP = (rec[5]&0x0F)*256+rec[4] + 9;
		sprintf((char *)&disp[4],"*Abs. pressure: %d, Rel. press.: %d", w->absP, w->relP);

		w->timestamp = t;
		break;
	case 0x48:{	// wind
		for(i = 2; i < WLEN; i++)
			csum += rec[i];
		if(csum != rec[WLEN]+rec[WLEN+1]*256)
			break;	// checksum error

		w->wind = ((rec[5] >> 4)+(rec[6]*16))*0.36;
		w->gust = ((rec[5] & 0x0F)*256+rec[4])*0.36;
		w->windDir = (rec[2] & 0x0F)*360/16;
		w->wBatt = flags >> 4;
		sprintf((char *)&disp[5],"*Wind gust: %3.1f avg: %3.1f %s %d Batt: %d", w->gust, w->wind,
				winddir[rec[2] & 0x0F], w->windDir, w->wBatt);
//		printBytes(&rec[0], 11);
//		printf("Wind gust: %3.1f avg: %3.1f %s Batt: %d\n", ((rec[5] & 0x0F)*256+rec[4])*0.36,
//				((rec[5] >> 4)+(rec[6]*16))*0.36, winddir[rec[2] & 0x0F], (flags>>4));

		w->timestamp = t;
		break;
		}
	case 0x41:	// rain
		for(i = 2; i < RLEN; i++)
			csum += rec[i];
		if(csum != rec[RLEN]+rec[RLEN+1]*256)
			break;	// checksum error

		w->prec = (rec[3]*256+rec[2])*0.254;
		w->prec1 = (rec[5]*256+rec[4])*0.254;
		w->prec24 = (rec[7]*256+rec[6])*0.254;
		w->precTot = (rec[9]*256+rec[8])*0.254;
		w->pBatt = flags >> 4;
		sprintf((char *)&disp[6],"*Rain %3.1f last hour %3.1f last 24 hours %3.1f", w->prec, w->prec1, w->prec24);
		sprintf((char *)&disp[7],"   Total %3.1f since %02d/%02d/%02d %02d:%02d Batt: %d", w->precTot,
				rec[14], rec[13], rec[12], rec[11], rec[10], w->pBatt);

		w->timestamp = t;
		break;
	}
}

int main(int argc, char *argv[])
{
    unsigned char buf[10], usbRecords[100];
    int j, usbRecordLength, shmid, first = 1, trfrd;
	struct tm *ptm;

    openlog(argv[0], 0, 0);
    syslog(LOG_INFO, "starting");

	fp = fopen("/run/shm/w.dat", "w");

	/* create shared memory for WMRS communication */
	if((shmid = shmget(1962, sizeof(wmrs_t), IPC_CREAT | 0666)) < 0){
		syslog(LOG_INFO, "shmget: %m");
		exit(EXIT_FAILURE);
	}
	if((w = shmat(shmid, NULL, 0)) == (void *)-1){
		syslog(LOG_INFO, "shmat: %m");
		exit(EXIT_FAILURE);
	}

	signal(SIGTERM, cleanup);

    if((j = libusb_init(NULL))){
		syslog(LOG_INFO, "libusb_init: %d", j);
		exit(EXIT_FAILURE);
    }

    libusb_set_debug(NULL, 3);

    rediscover:
    wmrs = libusb_open_device_with_vid_pid(NULL, 0x0FDE, 0xCA01);
    if(NULL == wmrs){
		syslog(LOG_INFO, "libusb_open");
		exit(EXIT_FAILURE);
    }

    if(libusb_kernel_driver_active(wmrs, 0) == 1){
        libusb_detach_kernel_driver(wmrs, 0);
    }

	if(libusb_claim_interface(wmrs, 0)) syslog(LOG_INFO, "claim interface");

	memcpy(buf, "\x20\x00\x08\x01\x00\x00\x00\x00", 8);
	j=libusb_control_transfer(wmrs, (0x01 << 5)+1,9,0x200,0,buf,8,1000);	// send init message
	for(ever){	// main loop
		sleep(1);
		// shift historical data buffer when hour changed
		t = time(NULL);
		ptm = localtime(&t);
		if(ptm->tm_min == 0){
			if(first){
				first = 0;
				for(j = 0; j < 23; j++){
					w->tHist[j] = w->tHist[j+1];
					w->rhHist[j] = w->rhHist[j+1];
				}
			}
		}
		else{
			first = 1;
		}
		// store indoor data
		w->tHist[23] = w->s[0].temp;
		w->rhHist[23] = w->s[0].rh;

		// read reports from device
		usbRecordLength = 0;
		while(!(j = libusb_bulk_transfer(wmrs,0x81,buf,8,&trfrd,1000)) && (trfrd == 8)){	// read full report
		    if(j < 0){  // error
		        if(libusb_reset_device(wmrs) == LIBUSB_ERROR_NOT_FOUND){
		            libusb_close(wmrs);
		            goto rediscover;
		        }
		        usbRecordLength = 0;
		        break;
		    }
			memcpy(&usbRecords[usbRecordLength], &buf[1], buf[0]);	// copy record fragment
			usbRecordLength += buf[0];
			usleep(100*1000); // wait 100ms
		}
		if(usbRecordLength == 0) continue;
//		printBytes(usbRecords, usbRecordLength);
		
		// process records
		for(j = 0; j < usbRecordLength; j++){
			if(usbRecords[j] == 0xFF && usbRecords[j+1] == 0xFF){	// record separator
				j += 2;	// skip separator
				if((j + 5) <= usbRecordLength) processRecord(&usbRecords[j]);
			}
		}
		
#ifdef FOXBOARD
		rewind(fp);
		for(j = 0; j < 8; j++){
			fputs((char *)&disp[j], fp);
			fputc('\n', fp);
			disp[j][0] = ' ';
		}
#else
		system("clear");
		for(j = 0; j < 8; j++){
			puts((char *)&disp[j]);
			disp[j][0] = ' ';
		}
#endif
	}
	return 0;
}
/* SDG */
