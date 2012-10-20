/* gamedb.h
 *
*/

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997  Erik Van Riper (geek@midway.com)
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

#ifndef GAMEDB_H_
#define GAMEDB_H_

	/* AvK Fixme: to avoid dependency on sys/types.h:ulong,
	** mink.h now supplies xulong. (typedef'd as unsigned long, anyway)
	** I don't think, the code relies on (ulong===xulong).
	*/
#include "mink.h"

#define GSTATUS_EMPTY 0
#define GSTATUS_NEW 1
#define GSTATUS_ACTIVE 2
#define GSTATUS_STORED 3
/* #define GSTATUS_ANALYSIS 4   for analysis mode */

#define MAX_GO_MOVES 1000

#define GAMETYPE_GO 1
#define GAMETYPE_GOEGO 2
#define GAMETYPE_NETGO 3

#define GAMETYPE_LADDER 4
#define GAMETYPE_TGO 5
#define GAMETYPE_TGOEGO 6 
#define GAMETYPE_TNETGO 7

#define END_RESIGN 0           /* A player typed "resign" */
#define END_FLAG 1             /* A player ran out of time */
#define END_ADJOURN 2          /* Players agreed to adjourn */
#define END_LOSTCONNECTION 3   /* A player lost connection */
#define END_NOTENDED 4         /* The game is in progress */
#define END_DONE 5             /* The game is finished, was scored */

#define PLAYER_BLACK 0
#define PLAYER_WHITE 1
#define PLAYER_NEITHER 2

#define NOTPAIRED	0
#define PAIR1		1
#define PAIR2		2

#define TIMETYPE_UNTIMED 0
#define TIMETYPE_TIMED 1

#ifndef TICSPERSEC
#define TICSPERSEC (10)
#define SECS2TICS(s) ((s)*TICSPERSEC)
#define TICS2SECS(t) ((t)/TICSPERSEC)
#endif

struct game {
	/* Key info */
  int gstatus;
  struct {
    unsigned in_use: 1;
    unsigned is_playing: 1;
    unsigned is_stored: 1;
  } slotstat;
#if WANT_PAIR
  int pairwith;
  int pairstate;
#endif
  char *gtitle;
  char *gevent;
  struct minkgame *minkg;
  time_t timeOfStart; 
  unsigned starttick;    /* The tic when this game was started  */
  unsigned lastMovetick; /* Last tic a move was made */
  unsigned lastDectick;  /* Last tic a players clock was decremented */
  int nmvinfos;
  struct mvinfo *mvinfos;
	/* flags */
  int teach;  /* 1=Single player teaching game presentation 2=2player-teach*/
  int rated;
  int Private;
  int Tourn;
  int Ladder9;
  int Ladder19;
  int Ladder_Possible;
	/* parameters */
  int size;
  int type;
  int gotype;
  int rules;         /* RULES_NET or RULES_ING */
  float komi;
  struct timestuff {
    int time_type;	/* If timed or untimed */
    int totalticks;	/* Total game time allowed */
    int byoticks;	/* Byo ticks per byo period */
    int byostones;	/* Byo stones per player */
    } ts;
	/* move state */
  int onMove;
  int num_pass; 
  struct playerstuff {
    int pnum;
    int old_pnum;	/* Contains the old game player number */
    int ticksleft;
    int byoperiods;		/* player in byo-yomi: number of byo periods entered */
    int byostones;	/* Stones left to play in byo period */
  /* GOE */
    int penalty;	/* number of penalty points */
    } black,white;
  int clockStopped;
  int nocaps;
	/* Result */
  int result;
  int winner;
  float gresult;
} ;

extern struct game garray[];
extern int garray_top;

int game_new(int, int);
int game_remove(int);
int game_finish(int);

char *game_time_str(int, int, int, int);
int game_isblitz(int, int, int, int, char *, char *);

void add_kib(struct game *g, int movecnt, char *s);

void send_go_board_to(int, int);
void send_go_boards(int, int);
void game_update_time(int);
void game_update_times(void);

	/* This is the number of games that is kept in
	** garray[], after they completed.
	** This should allow players to analyse or continue
	** after finishing or disconnect/reconnect
	*/
#define MAXOLDGAMES 50

int FindOldGameFor(int);
int RemoveOldGamesForPlayer(int);
int ReallyRemoveOldGamesForPlayer(int);
int NewOldGame(int);

void game_disconnect(int, int);

int game_read(struct game *, int, int);
int game_delete(int, int);
int game_save(int);

int pgames(int, FILE *);
void game_write_complete(int, twodstring);
unsigned int game_count(void);
int game_get_num_ob(int);
int write_g_out(int, FILE *, int, char *);
int game_save_complete(int, FILE *, twodstring);
#endif /* GAMEDB_H_ */

