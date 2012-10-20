/* nngsmain.h
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996  Erik Van Riper (geek@willent.com)
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

#ifndef NNGSMAIN_H
#define NNGSMAIN_H

/* Heartbeat functions occur approx in this time, including checking for
 * new connections and decrementing Ticsleft counters. */
#define HEARTBEATTIME 1

/* Number of seconds that an idle connection can stand at login or password
 * prompt. */
#if 0
#define MAX_LOGIN_IDLE (2*60)
#define MAX_IDLE (90*60)
#endif

#define NUM_LADDERS 2
#define LADDERSIZE 500 

#define DEFAULT_PROMPT "#> "
#define DEFAULT_KOMI19 "6.5"
#define DEFAULT_KOMI13 "8.5"
#define DEFAULT_KOMI9 "5.5"

#define LINESIZE        80
#define NUMLINES        24 /* Things break if larger, run out of screen space,
                              and those ascii client people trying to play
                              99x99 games (all talby's fault) */

#ifndef TICSPERSEC
#define TICSPERSEC (10)
#define SECS2TICS(s) ((s)*TICSPERSEC)
#define TICS2SECS(t) ((t)/TICSPERSEC)
#endif

typedef char twodstring[NUMLINES][LINESIZE];

#define RULES_NET 1
#define RULES_ING 2

struct kibitz {
  char *mess;
  struct kibitz *next;
} ;

struct mvinfo {
  struct kibitz *kibitz;
  struct kibitz **last;
} ;


/* This is the socket that the current command came in on */
extern int current_socket;

extern int Ladder9;
extern int Ladder19;
extern int num_19;
extern int num_9;
extern int completed_games;
extern int num_logins, num_logouts, new_players;
extern char orig_command[1024];

void main_exit (int code);

#endif /* NNGSMAIN_H */
