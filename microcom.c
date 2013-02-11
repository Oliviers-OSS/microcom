/******************************************************************
** File: microcom.c
** Description: the main file for microcom project
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
** Rev. 1.0 - Feb. 2000
** Rev. 1.01 - March 2000
** Rev. 1.02 - June 2000
****************************************************************************/
#include "microcom.h"

int crnl_mapping; //0 - no mapping, 1 mapping
int script = 0; /* script active flag */
char scr_name[MAX_SCRIPT_NAME] = "script.scr"; /* default name of the script */
char device[MAX_DEVICE_NAME]; /* serial device name */
int log = 0; /* log active flag */
FILE* flog;   /* log file */
int  pf = 0;  /* port file descriptor */
struct termios pots; /* old port termios settings to restore */
struct termios sots; /* old stdout/in termios settings to restore */




void init_comm(struct termios *pts) {
   /* some things we want to set arbitrarily */
   pts->c_lflag &= ~ICANON; 
   pts->c_lflag &= ~(ECHO | ECHOCTL | ECHONL);
   pts->c_cflag |= HUPCL;
   pts->c_cc[VMIN] = 1;
   pts->c_cc[VTIME] = 0;
   
   /* Standard CR/LF handling: this is a dumb terminal.
    * Do no translation:
    *  no NL -> CR/NL mapping on output, and
    *  no CR -> NL mapping on input.
    */
   pts->c_oflag |= ONLCR;
   crnl_mapping = 1;
   pts->c_iflag &= ~ICRNL;

  /* set hardware flow control by default */
  pts->c_cflag |= CRTSCTS;
  pts->c_iflag &= ~(IXON | IXOFF | IXANY);
  /* set 9600 bps speed by default */
  cfsetospeed(pts, B9600);
  cfsetispeed(pts, B9600);
  
}

void init_stdin(struct termios *sts) {
   /* again, some arbitrary things */
   sts->c_iflag &= ~BRKINT;
   sts->c_iflag |= IGNBRK;
   sts->c_lflag &= ~ISIG;
   sts->c_cc[VMIN] = 1;
   sts->c_cc[VTIME] = 0;
   sts->c_lflag &= ~ICANON;
   /* no local echo: allow the other end to do the echoing */
   sts->c_lflag &= ~(ECHO | ECHOCTL | ECHONL);
}


 


/********************************************************************
 Main functions
 ********************************************************************
 static void help_usage(int exitcode, char *error, char *addl)
      help with running the program
      - exitcode - to be returned when the program is ended
      - error - error string to be printed
      - addl - another error string to be printed
 static void cleanup_termios(int signal)
      signal handler to restore terminal set befor exit
 int main(int argc, char *argv[]) -
      main program function
********************************************************************/
void main_usage(int exitcode, char *str, char *dev) {
  fprintf(stderr, "Usage: microcom [options]\n"
	  " [options] include:\n"
	  "    -Ddevfile       use the specified serial port device;\n" 
          "                    if a port is not provided, microcom\n"
	  "                        will try to autodetect a modem\n"
	  "           example: -D/dev/ttyS3\n"
	  "    -S              run script from script.scr (default)\n"
	  "    -Sscrfile       run script from scrfile\n\n"
	  "microcom provides session logging in microcom.log file\n");
  fprintf(stderr, "Exitcode %d - %s %s\n\n", exitcode, str, dev);
  exit(exitcode);
}

/* restore original terminal settings on exit */
void cleanup_termios(int signal) {
  /* cloase the log file first */
  if (log) {
    fflush(flog);
    fclose(flog);
  }
  tcsetattr(pf, TCSANOW, &pots);
  tcsetattr(STDIN_FILENO, TCSANOW, &sots);
  exit(0);
} 


int main(int argc, char *argv[]) {
  struct  termios pts;  /* termios settings on port */
  struct  termios sts;  /* termios settings on stdout/in */
  struct sigaction sact;/* used to initialize the signal handler */
  int i;
  
  device[0] = '\0';

  /* parse command line */
  for (i = 1; i < argc; i++) {
    if (strncmp(argv[i], "-S", 2) == 0) {
      script = 1; /* set script flag */
      if (argv[i][2] != '\0') /* we have a new scriptname */
	strncpy(scr_name, &argv[1][2], MAX_SCRIPT_NAME);
      continue;
    }
    if (strncmp(argv[i], "-D", 2) == 0) {
      if (argv[i][2] != '\0') /* we have a device */
	strncpy(device, &argv[i][2], MAX_DEVICE_NAME);
      continue;
    }
    if (strncmp(argv[i], "-?", 2) == 0 ||
	strncmp(argv[i], "-H", 2) == 0)
      main_usage(0, "", "");
  }
   

  if (strlen(device) == 0) {
    /* if no device was passed on the command line,
       try to autodetect a modem */
    for (i = 0; i < 4; i++)
      if (autodetect(i)) {
	sprintf(device, "/dev/ttyS%1d", i);
	break;
      }
    if (i == 4)
      main_usage(1, "no device found", "");
  }

  /* open the device */
  pf = open(device, O_RDWR);
  if (pf < 0)
    main_usage(2, "cannot open device", device);


  /* modify the port configuration */
  tcgetattr(pf, &pts);
  memcpy(&pots, &pts, sizeof(pots));
  init_comm(&pts);
  tcsetattr(pf, TCSANOW, &pts);
     
  /* Now deal with the local terminal side */
  tcgetattr(STDIN_FILENO, &sts);
  memcpy(&sots, &sts, sizeof(sots)); /* to be used upon exit */
  init_stdin(&sts);
  tcsetattr(STDIN_FILENO, TCSANOW, &sts);

  /* set the signal handler to restore the old
   * termios handler */
  sact.sa_handler = cleanup_termios; 
  sigaction(SIGHUP, &sact, NULL);
  sigaction(SIGINT, &sact, NULL);
  sigaction(SIGPIPE, &sact, NULL);
  sigaction(SIGTERM, &sact, NULL);

  /* run thhe main program loop */
  mux_loop(pf);

  /* restore original terminal settings and exit */
  tcsetattr(pf, TCSANOW, &pots);
  tcsetattr(STDIN_FILENO, TCSANOW, &sots);

  exit(0);

}





