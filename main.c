/** \file clients/lcdproc/main.c
 * Contains main(), plus signal callback functions and a help screen.
 *
 * Program init, command-line handling, and the main loop are
 * implemented here.
 */

/*-
 * This file is part of lcdproc, the lcdproc client.
 *
 * This file is released under the GNU General Public License.
 * Refer to the COPYING file distributed with this package.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/param.h>

#include "main.h"
#include "sockets.h"

/* Import screens */
#include "exec.h"

/* The following 8 variables are defined 'external' in main.h! */
int Quit = 0;
int sock = -1;

char *version = "0.1";
char *build_date = __DATE__;

int lcd_wid = 0;
int lcd_hgt = 0;
int lcd_cellwid = 0;
int lcd_cellhgt = 0;

static struct utsname unamebuf;

/* local prototypes */
static void exit_program(int val);
static void main_loop(void);


#define TIME_UNIT	125000	/**< 1/8th second is a single time unit. */

#if !defined(SYSCONFDIR)
# define SYSCONFDIR	"/etc"
#endif
#if !defined(PIDFILEDIR)
# define PIDFILEDIR	"/var/run"
#endif

#define UNSET_INT		-1
#define UNSET_STR		"\01"
#define DEFAULT_SERVER		"127.0.0.1"
#define DEFAULT_CONFIGFILE	SYSCONFDIR "/lcdproc.conf"
#define DEFAULT_PIDFILE		PIDFILEDIR "/lcdproc.pid"
#define DEFAULT_REPORTDEST	RPT_DEST_STDERR
#define DEFAULT_REPORTLEVEL	RPT_WARNING

/** list of screen modes to run */
ScreenMode sequence[] =
{
   /* flags default ACTIVE will run by default */
   /* longname    which on  off inv  timer   flags */
   { "ExecLCD", 'E',   64,   64, 0, 0xffff, ACTIVE,      exec_command },
   {  NULL, 0, 0, 0, 0, 0, 0, NULL},			  	// No more..  all done.
};


/* All variables are set to 'unset' values */
static int islow = -1;		/**< pause after mode update (in 1/100s) */
char *progname = "lcdproc";
char *server = NULL;
int port = LCDPORT;
int foreground = FALSE;
char *configfile = NULL;
char *pidfile = NULL;
int pidfile_written = FALSE;
char *displayname = NULL;	/**< display name for the main menu */

/** Returns the network name of this machine */
const char * get_hostname(void) {
   return (unamebuf.nodename);
}

int main(int argc, char **argv) {

   /* get uname information */
   if (uname(&unamebuf) == -1) {
      perror("uname");
      return (EXIT_FAILURE);
   }

   /* setup error handlers */
   signal(SIGINT, exit_program);	/* Ctrl-C */
   signal(SIGTERM, exit_program);	/* "regular" kill */
   signal(SIGHUP, exit_program);	/* kill -HUP */
   signal(SIGPIPE, exit_program);	/* write to closed socket */
   signal(SIGKILL, exit_program);	/* kill -9 [cannot be trapped; but ...] */

   if (server == NULL)
      server = DEFAULT_SERVER;

   /* Connect to the server... */
   sock = sock_connect(server, port);
   if (sock < 0) {
      fprintf(stderr, "Error connecting to LCD server %s on port %d.\n"
            "Check to see that the server is running and operating normally.\n",
            server, port);
      return (EXIT_FAILURE);
   }

   sock_send_string(sock, "hello\n");
   usleep(500000);		/* wait for the server to say hi. */

   /* We grab the real values below, from the "connect" line. */
   lcd_wid = 20;
   lcd_hgt = 4;
   lcd_cellwid = 5;
   lcd_cellhgt = 8;

   foreground = TRUE;
   if (foreground != TRUE) {
      if (daemon(1, 0) != 0) {
         fprintf(stderr, "Error: daemonize failed\n");
         return (EXIT_FAILURE);
      }

      if (pidfile != NULL) {
         FILE *pidf = fopen(pidfile, "w");

         if (pidf) {
            fprintf(pidf, "%d\n", (int)getpid());
            fclose(pidf);
            pidfile_written = TRUE;
         }
         else {
            fprintf(stderr, "Error creating pidfile %s: %s\n",
                  pidfile, strerror(errno));
            return (EXIT_FAILURE);
         }
      }
   }

   /* And spew stuff! */
   main_loop();
   exit_program(EXIT_SUCCESS);

   /* NOTREACHED */
   return EXIT_SUCCESS;
}


/** Called upon TERM and INTR signals. Also removes the pid-file. */
void exit_program(int val) {
   Quit = 1;
   sock_close(sock);
   if ((foreground != TRUE) && (pidfile != NULL) && (pidfile_written == TRUE))
      unlink(pidfile);
   exit(val);
}

/**
 * Calls the mode specific screen init / update function and updates the Eyebox
 * screen as well. Sets the backlight state according to return value of the
 * mode specific screen function.
 *
 * \param m        The screen mode
 * \param display  Flag whether to update screen even if not visible.
 * \return  Backlight state
 */
int update_screen(ScreenMode *m, int display) {
   static int status = -1;
   int old_status = status;

   if (m && m->func) {
      status = m->func(m->timer, display, &(m->flags));
   }

   if (status != old_status) {
      if (status == BACKLIGHT_OFF)
         sock_send_string(sock, "backlight off\n");
      if (status == BACKLIGHT_ON)
         sock_send_string(sock, "backlight on\n");
      if (status == BLINK_ON)
         sock_send_string(sock, "backlight blink\n");
   }

   return (status);
}


void main_loop(void) {
   int i = 0, j;
   int connected = 0;
   char buf[8192];
   char *argv[256];
   int argc, newtoken;
   int len;

   while (!Quit) {
      /* Check for server input... */
      len = sock_recv(sock, buf, 8000);

      /* Handle server input... */
      while (len > 0) {
         /* Now split the string into tokens... */
         argc = 0;
         newtoken = 1;

         for (i = 0; i < len; i++) {
            switch (buf[i]) {
               case ' ':
                  newtoken = 1;
                  buf[i] = 0;
                  break;
               default:	/* regular chars, keep
                         * tokenizing */
                  if (newtoken)
                     argv[argc++] = buf + i;
                  newtoken = 0;
                  break;
               case '\0':
               case '\n':
                  buf[i] = 0;
                  if (argc > 0) {
                     if (0 == strcmp(argv[0], "listen")) {
                        for (j = 0; sequence[j].which; j++) {
                           if (sequence[j].which == argv[1][0]) {
                              sequence[j].flags |= VISIBLE;
                           }
                        }
                     }
                     else if (0 == strcmp(argv[0], "ignore")) {
                        for (j = 0; sequence[j].which; j++) {
                           if (sequence[j].which == argv[1][0]) {
                              sequence[j].flags &= ~VISIBLE;
                           }
                        }
                     }
                     else if (0 == strcmp(argv[0], "key")) {
                     }
                     else if (0 == strcmp(argv[0], "menu")) {
                     }
                     else if (0 == strcmp(argv[0], "connect")) {
                        int a;
                        for (a = 1; a < argc; a++) {
                           if (0 == strcmp(argv[a], "wid"))
                              lcd_wid = atoi(argv[++a]);
                           else if (0 == strcmp(argv[a], "hgt"))
                              lcd_hgt = atoi(argv[++a]);
                           else if (0 == strcmp(argv[a], "cellwid"))
                              lcd_cellwid = atoi(argv[++a]);
                           else if (0 == strcmp(argv[a], "cellhgt"))
                              lcd_cellhgt = atoi(argv[++a]);
                        }
                        connected = 1;
                        if (displayname != NULL)
                           sock_printf(sock, "client_set -name \"%s\"\n", displayname);
                        else
                           sock_printf(sock, "client_set -name {LCDproc %s}\n", get_hostname());
                     }
                     else if (0 == strcmp(argv[0], "bye")) {
                        exit_program(EXIT_SUCCESS);
                     }
                     else if (0 == strcmp(argv[0], "success")) {
                     }
                     else {
                     }
                  }

                  /* Restart tokenizing */
                  argc = 0;
                  newtoken = 1;
                  break;
            }	/* switch( buf[i] ) */
         }

         len = sock_recv(sock, buf, 8000);
      }

      /* Gather stats and update screens */
      if (connected) {
         for (i = 0; sequence[i].which > 0; i++) {
            sequence[i].timer++;

            if (!(sequence[i].flags & ACTIVE))
               continue;

            if (sequence[i].flags & VISIBLE) {
               if (sequence[i].timer >= sequence[i].on_time) {
                  sequence[i].timer = 0;
                  /* Now, update the screen... */
                  update_screen(&sequence[i], 1);
               }
            }
            else {
               if (sequence[i].timer >= sequence[i].off_time) {
                  sequence[i].timer = 0;
                  /* Now, update the screen... */
                  update_screen(&sequence[i], sequence[i].show_invisible);
               }
            }
            if (islow > 0)
               usleep(islow * 10000);
         }
      }

      /* Now sleep... */
      usleep(TIME_UNIT);
   }
}

