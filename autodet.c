/******************************************************************
** File: autodet.c
** Description: implements modem autodetect facility
**
** Copyright (C)1999 Anca and Lucian Jurubita <ljurubita@hotmail.com>.
** All rights reserved.
****************************************************************************
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details at www.gnu.org
****************************************************************************
** Rev. 1.02 - June 2000
****************************************************************************/
#include "microcom.h"


#define LOCAL_BUF 25
int autodetect(int port) {
  int pf; /* port handler */
  int     i = 0;        /* used in the multiplex loop */
  int retval = 0;
  
  fd_set  ready;        /* used for select */
  struct timeval tv;

  char    buf[LOCAL_BUF + 1]; /* we'll add a '\0' */
  char device[LOCAL_BUF];
  struct  termios pts;  /* new comm port settings */
  struct termios pots; /*old comm port settings */
  struct  termios sts;  /* new stdin settings */
  struct termios sots; /* old stdin settings */
  
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  sprintf(device, "/dev/ttyS%1d", port);
  printf("Try %s\n", device);

  pf = open(device, O_RDWR);
  if (pf < 0) {
     printf("%s not found\n", device);
     return 0;
  }

   /* modify comm port configuration */
  tcgetattr(pf, &pts);
  memcpy(&pots, &pts, sizeof(pots)); /* to be used upon exit */
  init_comm(&pts);
  tcsetattr(pf, TCSANOW, &pts);

  /* modify stdin configuration */
  tcgetattr(STDIN_FILENO, &sts);
  memcpy(&sots, &sts, sizeof(sots)); /* to be used upon exit */
  init_stdin(&sts);
  tcsetattr(STDIN_FILENO, TCSANOW, &sts);
   
  /* send an at&f and wait for an OK */
  FD_ZERO(&ready);
  FD_SET(pf, &ready);
  write(pf, "at&f\n", 5);
  select(pf+1, &ready, NULL, NULL, &tv);
  if (FD_ISSET(pf, &ready)) {
    sleep(2);
    /* pf has characters for us */
    i = read(pf, buf, LOCAL_BUF);
  }

  if (i > 0) {
    buf[i + 1] = '\0';
    printf(buf);
    if (strstr(buf, "OK") != NULL) {
      printf("Modem found on %s\n\n", device);
      retval = 1;
    }
  }
  else {
    printf("%s not responding\n\n", device);
  }

  tcsetattr(pf, TCSANOW, &pots);
  tcsetattr(STDIN_FILENO, TCSANOW, &sots);

  close(pf);
  return retval;
}

