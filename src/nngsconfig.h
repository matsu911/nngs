/* nngsconfig.h

    NNGS - The No Name Go Server
    Copyright (C) 1995  Erik Van Riper (geek@imageek.york.cuny.edu)
    and John Tromp (tromp@daisy.uwaterloo.ca/tromp@cwi.nl)

    Adapted from:
    fics - An internet chess server.
    Copyright (C) 1993  Richard V. Nash

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifndef NNGSCONFIG_H
#define NNGSCONFIG_H

/* CONFIGURE THIS: The port on which the server binds */
#define DEFAULT_PORT 9696

/* If you want to have a running byte count */
#define WANT_BYTE_COUNT 1

/* Set this to MODE_FOR_DIR !=0 if you want to create non-existing 
** directories on the fly. This allows you to start without the
** {help,player,games} files in the right place, at the risk of
** creating them at the wrong places. More specifically: all over
** the place. Use with care.
** */
/*define MODE_FOR_DIR 0755 */
#define MODE_FOR_DIR 0

/* AvK this is for regression testing.
** If set, it attempts to imitate the
** behavior of nngs build #1205 (which I use as a reference)
** , even if that is wrong.
*/
#define MIMIC_NNGS_1205 1

/* The name of the server */
#define SERVER_NAME "NNGS unconfigured luser clone"
#define SERVER_ADDRESS "nngs.localhost"
#define SERVER_PORTS "6969,9696"
#define SERVER_EMAIL "nngs@localhost"
#define SERVER_HTTP "http://localhost"
#define GEEK_EMAIL NULL
#define ADMIN_NAME "admin"


/* define this to sift the ladder around */
#define WANT_LADDER_SIFT 1

/* define this if you want pair-go compiled in */
#define WANT_PAIR 1

/* define this if you have a ratings file somewhere.  See playerdb.c */
#define WANT_NNGSRATED 0
/*#undef WANT_NNGSRATED*/


/* define this if we want the "news" (bulleting boards). */
/* [PEM]: Disabled the news commands. They are undocumented, practically
   never used, and partially very buggy. ("anews all" breaks clients for
   instance.) Must be fixed and documented before enabled again. */
/* #define NEWS_BB 1 */

/* This is the length of the password you wish to generate */
#define PASSLEN 5

/* Where the standard ucb mail program is */
/* autoconfed now
#ifdef SGI
#define MAILPROGRAM "/usr/sbin/Mail"
#elif LINUX
#define MAILPROGRAM "/bin/mail"
#elif FREEBSD
#define MAILPROGRAM "/usr/bin/mail"
#else 
#define MAILPROGRAM "/bin/mailx"
#endif
*/

/* May need this, like on SGI, for instance. */
#ifdef SGI
int   strcasecmp(const char *, const char *);
int   strncasecmp(const char *, const char *, size_t);
FILE *popen(const char *, const char *);
int   pclose(FILE *);
int gettimeofday(struct timeval *tp,...);

 /* This should be in stdio.h */
#ifdef HUH
int      fileno(FILE *);
#endif

#endif /* SGI */

#endif /* NNGSCONFIG_H */
