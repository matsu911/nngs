/*
    NNGS - The No Name Go Server
    Copyright (C) 1995  J. Alan Eldridge (alane@wozzle.york.cuny.edu)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "utils.h"
#include "common.h"
#include "ladder.h"

struct ladder {
  int n;
  int max;
  int fNameSorted;
  struct ladderplayer **byPosn;
  struct ladderplayer **byName;
  };

int dbmax;
static struct ladder *db;

static int ladder_cmp_name(const void *p1, const void *p2);
static void copy(int idx, int dst, int src);
static void ladder_dump(FILE *pf, const struct ladderplayer *p);
static int ladder_new_local(int idx, int n);
static const struct ladderplayer * const *ladder_by_pos(int id);
/* just the array, in order, with a lot of const shit so you can't f*** it up */

void ladder_delete(int idx)
{
  if (db[idx].max) {
    int i;

    for (i = 0; i < db[idx].n; i++) {
      free(db[idx].byPosn[i]);
    }

    free(db[idx].byPosn);
  }

  db[idx].n = db[idx].max = 0;
  db[idx].byPosn = db[idx].byName = 0;
}

void ladder_init(int n)
{
  dbmax = n;
  db = malloc(dbmax * sizeof *db);
  memset(db ,0, dbmax * sizeof *db);
  assert(db != NULL);
}


static int ladder_new_local(int idx, int n)
{
  db[idx].n = 0;
  db[idx].max = n;
  db[idx].fNameSorted = 0;
  db[idx].byPosn = malloc(n * sizeof *db[idx].byPosn);
  db[idx].byName = malloc(n * sizeof *db[idx].byName);

  assert(db[idx].byPosn != NULL);
  assert(db[idx].byName != NULL);

  memset(db[idx].byPosn ,0, n* sizeof *db[idx].byPosn);
  memset(db[idx].byName ,0, n* sizeof *db[idx].byName);

  return idx;
}

int ladder_new(int n)
{
  int idx;

  for (idx = 0; idx < dbmax; idx++) {
    if (db[idx].max == 0) break;
  }

  if (idx == dbmax) return 0;

  return ladder_new_local(idx, n);
}


const struct ladderplayer *ladder_player_at(int idx, int i)
{
  if (i >= 0 && i < db[idx].n) return db[idx].byPosn[ i ];
  return 0;
}


void ladder_set_time(int idx, int i, time_t t)
{
  if (i >= 0 && i < db[idx].n) db[idx].byPosn[i]->lasttime = t;
}


void ladder_win(int idx, int i)
{
  if (i >= 0 && i < db[idx].n) db[idx].byPosn[i]->nWins++;
}

void ladder_lose(int idx, int i)
{
  if (i >= 0 && i < db[idx].n) db[idx].byPosn[i]->nLosses++;
}

static void copy(int idx, int dst, int src)
{
  db[idx].byPosn[ dst ] = db[idx].byPosn[ src ];
  db[idx].byPosn[ dst ]->idx = dst;
}

void ladder_remove_at(int idx, int target)
{
  if (target >= 0 && target < db[idx].n) {
    int i;
    int lim = --db[idx].n;

    db[idx].fNameSorted = 0;
    free(db[idx].byPosn[ target ]);
    for (i = target; i < lim; i++) {
      copy(idx, i, i + 1);
    }
  }
}

void ladder_rotate(int idx, int from, int to)
{
  if (from >= 0 && to >= 0 && from < db[idx].n && to < db[idx].n 
      && from < to) {
    int i;
    int lim = from + 1;
    struct ladderplayer *p = db[idx].byPosn[ to ];

    for (i = to; i >= lim; i--) {
      copy(idx, i, i - 1);
    }

    db[idx].byPosn[ from ] = p; p->idx = from;
  }
}

const struct ladderplayer *ladder_player_named(int idx, const char *psz)
{
  int i;

  for (i = 0; i < db[idx].n; i++) {
    if (strcmp(db[idx].byPosn[ i ]->name, psz)) continue;
    return db[idx].byPosn[ i ];
  }

  return 0;
}

const struct ladderplayer * const *ladder_by_pos(int idx)
{
  return db[idx].n ? (const struct ladderplayer * const *)db[idx].byPosn : NULL;
}

static int ladder_cmp_name(const void *p1, const void *p2)
{
#define P1 (*(const struct ladderplayer * const *)p1)
#define P2 (*(const struct ladderplayer * const *)p2)

  return strcmp(P1->name, P2->name);

#undef P1
#undef P2  
}

const struct ladderplayer * const *ladder_by_name(int idx)
{
  if (db[idx].n) {
    if (!db[idx].fNameSorted) {
      db[idx].fNameSorted = 1;
      memset(db[idx].byName, 0, db[idx].max * sizeof *db[idx].byName);
      memcpy(db[idx].byName, db[idx].byPosn, db[idx].n * sizeof *db[idx].byName);
      qsort(db[idx].byName, db[idx].n, sizeof *db[idx].byName, ladder_cmp_name);
    }
    return (const struct ladderplayer * const *)db[idx].byName;
  }

  return 0;
}

int ladder_new_p(int idx, const char *psz)
{
  struct ladderplayer *p;

  if (db[idx].n >= db[idx].max) return 0;

  assert(p = malloc(sizeof *p));
  memset(p, 0, sizeof *p);

  p->lasttime = 0;
  p->payload = 0;
  p->name = mystrdup(psz);
  p->nWins = p->nLosses = 0;
  db[idx].byPosn[ p->idx = db[idx].n++ ] = p;
  db[idx].fNameSorted = 0;

  return 1;
}

static void ladder_dump(FILE *pf, const struct ladderplayer *pp)
{
  fprintf(pf, "%03d \"%s\" %lu %d %d\n", pp->idx, pp->name, pp->lasttime,
	  pp->nWins, pp->nLosses);
}

int ladder_save(FILE *pf, int idx)
{
  int i;

  fprintf(pf, "Max: %d\n", db[idx].max);
  for (i = 0; i < db[idx].n; i++) {
    ladder_dump(pf, db[idx].byPosn[ i ]);
  }
  return db[idx].n;
}

static void setstats(int idx, int at, int wins, int losses)
{
  struct ladderplayer *p = db[idx].byPosn[at];

  p->nWins = wins;
  p->nLosses = losses;
}

int ladder_load(FILE *pf, int idx)
{
  int max;
  time_t lasttime;
  char name[ 100 ];

  char linebuff[ 200 ];

  if (fgets(linebuff, sizeof linebuff, pf)) { 
    int i = 0, dummy, nWins, nLosses;
    if (0>= sscanf(linebuff, "Max: %d", &max)) return -1;

    ladder_delete(idx);
    ladder_new_local(idx, max);
    while (fgets(linebuff, sizeof linebuff, pf)) {
      if (0 >= sscanf(linebuff, "%d \"%[^\"]\" %lu %d %d", 
		  &dummy, name, &lasttime, &nWins, &nLosses) ) break;
      ladder_new_p(idx, name);
      ladder_set_time(idx, i, lasttime);
      setstats(idx, i++, nWins, nLosses);
    }

    return db[idx].n;
  }
  return -1;
}

static void renumber(int idx)
{
  int i;
  for (i = 0; i < db[idx].n ; i++) {
    db[idx].byPosn[i]->idx = i;
  }
}

void ladder_sift(int idx, int nDays)
{
  int n;
  int nCurr;
  int *pCurr;
  int nLate;
  int *pLate;
  time_t tNow = globclock.time;
#define SECSPERDAY (24 * 60 * 60)
  const time_t cuttime = tNow - (nDays * SECSPERDAY);
  struct ladderplayer **pp;

  nCurr = nLate = 0;
  pCurr = malloc(db[idx].n * sizeof *pCurr);
  memset(pCurr ,0, db[idx].n * sizeof *pCurr);
  pLate = malloc(db[idx].n * sizeof *pLate);
  memset(pLate ,0, db[idx].n * sizeof *pLate);
  pp = malloc(db[idx].n * sizeof *pp);
  memset(pp ,0, db[idx].n * sizeof *pp);

  for (n = 0; n < db[idx].n ; n++) {
    if ((db[idx].byPosn[n]->lasttime < cuttime) || 
       ((db[idx].byPosn[n]->nWins == 0)   && 
        (db[idx].byPosn[n]->nLosses == 0))) {
      pLate[ nLate++ ] = n;
    } else {
      pCurr[ nCurr++ ] = n;
    }
  }

  for (n = 0; n < nCurr; n++) {
    pp[ n ] = db[idx].byPosn[ pCurr[n] ];
  }
  for (n = 0; n < nLate; n++) {
    pp[ n + nCurr ] = db[idx].byPosn[ pLate[n] ];
    /*pp[ n + nCurr ]->lasttime = tNow - SECSPERDAY;*/
  }
  memcpy(db[idx].byPosn, pp, db[idx].n * sizeof *db[idx].byPosn );
  renumber(idx);
  free(pp);
  free(pCurr);
  free(pLate);
#undef SECSPERDAY
}

