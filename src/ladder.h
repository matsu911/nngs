/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997  J. Alan Eldridge (alane@wozzle.york.cuny.edu)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifndef LADDER_H
#define LADDER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>   /* for the FILE declarations below */

#ifdef HAVE_TIME_H
#include <time.h>
#endif


struct ladderplayer {
  int idx;
  char *name;
  time_t lasttime;
  void *payload; /* unused */
  int nWins, nLosses;
} ;

void ladder_init(int nLadders);

int ladder_new(int maxPlayers);
void ladder_delete(int id);

int LadderCnt(int id);

const struct ladderplayer *ladder_player_at(int id, int at);

void ladder_sift(int idx, int nDays);
void ladder_set_time(int id, int at, time_t t);
void PlayerSetData(int id, int at, void *pv);
void ladder_win(int id, int at);
void ladder_lose(int id, int at);

void ladder_remove_at(int id, int at);
void ladder_rotate(int id, int from, int to);

const struct ladderplayer *ladder_player_named(int id, const char *psz);


const struct ladderplayer * const *ladder_by_name(int id);

int ladder_new_p(int id, const char *szName); /* add at end - return 0 if full */


int ladder_save(FILE *pf, int id);
int ladder_load(FILE *pf, int id);

#endif /* LADDER_H */
