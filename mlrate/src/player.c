/* $Id: player.c,v 1.3 2002/01/27 17:56:52 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12-12
**
** Players are kept in a straight vector in the order they
** appear on input. Pindex/Gindex is the next free slot. The vector
** grows when needed.
** Each player has a vector of games, which also grows when needed.
**
** Only indices are used to refer to players (and games) so they
** are relocable (for the growing).
**
** In addition there is a hash table mapping player names to indicies,
** for fast lookup during reading/insertion.
**
**   Copyright (C) 1998-2002  Per-Erik Martin
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include "player.h"
#include "game.h"
#include "rank.h"
#include "shash.h"
#include "error.h"

#define PLAYERS_INIT_SIZE  1024
#define PGAMES_INIT_SIZE   128

typedef struct
{
  char name[PLAYER_MAX_NAMELEN+1];
  irank_t rank;			/* The rating. */
  int rated;			/* Player has a rating (1 or 0) */
  int ignore;			/* Ignore this player (1 or 0) */
  unsigned wins, losses;	/* Rated wins/losses. */
  double wwins, wlosses;	/* Weighted wins/losses. */
  double low, high;
  game_t *games;		/* The game vector. */
  size_t gsize, gindex;		/* Size and index of the game vector. */
  time_t lastplayed;
} ps_t;

/* The indirect player vector, used by the iterator only, and can be
** resorted to get different iteration sequences.
*/
static player_t *Pindir = NULL;
static size_t Pindsize = 0, Pindindex = 0;

/* The player vector. */
static ps_t *Players = NULL;
static size_t Psize = 0, Pindex = 0;

/* The hash table of name->index. */
static shash_t Phash = NULL;

player_t
player_add(char *name, irank_t rank, int rated, int ignore)
{
  player_t pold;

  if (!Phash)
  {
    Phash = shash_create(2*PLAYERS_INIT_SIZE);
    if (!Phash)
      errex("shash_create(%lu) failed\n", 2*PLAYERS_INIT_SIZE);
  }
  if (shash_get(name, (void *)&pold, Phash))
    return pold;
  if (Pindindex == Pindsize)
  {
    Pindsize = (Pindsize == 0 ? PLAYERS_INIT_SIZE : 2 * Pindsize);
    Pindir = (player_t *)realloc(Pindir, Pindsize * sizeof(player_t));
    if (!Pindir)
      errex("realloc(...,%lu) failed\n", Pindsize * sizeof(player_t));
  }
  if (Pindex == Psize)
  {
    Psize = (Psize == 0 ? PLAYERS_INIT_SIZE : 2 * Psize);
    Players = (ps_t *)realloc(Players, Psize * sizeof(ps_t));
    if (!Players)
      errex("realloc(...,%lu) failed\n", Psize * sizeof(ps_t));
  }
  strncpy(Players[Pindex].name, name, PLAYER_MAX_NAMELEN);
  Players[Pindex].name[PLAYER_MAX_NAMELEN] = '\0';
  Players[Pindex].rank = (rated ? rank : 0.0);
  Players[Pindex].rated = rated; /* Anchor's are rated. */
  Players[Pindex].ignore = ignore;
  Players[Pindex].wins = 0;
  Players[Pindex].losses = 0;
  Players[Pindex].wwins = 0.0;
  Players[Pindex].wlosses = 0.0;
  Players[Pindex].lastplayed = 0;
  Players[Pindex].low = 0.0;
  Players[Pindex].high = 0.0;
  Players[Pindex].gsize = Players[Pindex].gindex = 0;
  Players[Pindex].games = NULL;

  if (!shash_put(name, (void *)Pindex, Phash))
    errex("shash_put(\"%s\", ...) failed\n", name);

  Pindir[Pindindex++] = Pindex;
  return Pindex++;
}

void
player_game_add(player_t pl, game_t g)
{
  ps_t *p = Players + pl;

  if (p->gindex == p->gsize)
  {
    p->gsize = (p->gsize == 0 ? PGAMES_INIT_SIZE : 2 * p->gsize);
    p->games = (game_t *)realloc(p->games, p->gsize * sizeof(game_t));
    if (!p->games)
      errex("realloc(...,%lu) failed\n", p->gsize * sizeof(game_t));
  }
  p->games[p->gindex++] = g;
  if (p->lastplayed < game_date(g))
    p->lastplayed = game_date(g);
}

/*
**  Accessors.
*/

int
player_get(char *name, player_t *p)
{
  if (Phash)
    return shash_get(name, (void **)p, Phash);
  return 0;
}

size_t
player_count(void)
{
  return Pindex;
}

#define PGET(Type,Field,Default) \
Type \
player_get_ ## Field (player_t p) \
{ \
  assert(Players && p < Pindex); \
  return Players[p].Field; \
}

PGET(char *,name,NULL)
PGET(irank_t,rank,(irank_t)0)
PGET(int,rated,0)
PGET(int,ignore,0)
PGET(unsigned,wins,0)
PGET(unsigned,losses,0)
PGET(double,wwins,0)
PGET(double,wlosses,0)
PGET(double,low,0.0)
PGET(double,high,0.0)
PGET(time_t,lastplayed,0)

#undef PGET

unsigned
player_get_ratedgames(player_t p)
{
  assert(Players && p < Pindex);
  return Players[p].wins + Players[p].losses;
}


/*
**  Destructors.
*/

#define PSET(Type,Field) \
void \
player_set_## Field (player_t p, Type v)\
{ \
  assert(Players && p < Pindex); \
  Players[p].Field = v; \
}

PSET(irank_t,rank)
PSET(int,rated)
PSET(int,ignore)
PSET(unsigned,wins)
PSET(unsigned,losses)
PSET(double,wwins)
PSET(double,wlosses)
PSET(double,low)
PSET(double,high)
PSET(time_t,lastplayed)

#undef PSET

void
player_set_ratedgames(player_t p, unsigned wins, unsigned losses)
{
  assert(Players && p < Pindex);
  Players[p].wins = wins;
  Players[p].losses = losses;
}

void
player_set_wratedgames(player_t p, double wins, double losses)
{
  assert(Players && p < Pindex);
  Players[p].wwins = wins;
  Players[p].wlosses = losses;
}


/*
**  Iterators.
*/

void
player_start(piter_t *pitp)
{
  *pitp = 0;
}

player_t
player_next(piter_t *ip)
{
  assert(Pindir);
  if (ip && *ip < Pindindex)
    return Pindir[(*ip)++];
  return NO_PLAYER;
}

void
player_games_start(player_t p, giter_t *gitp)
{
  gitp->i = 0;
  gitp->p = p;
}

game_t
player_games_next(giter_t *giter)
{
  assert(Players);
  if (giter)
  {
    ps_t *p = Players + giter->p;

    if (p->games && giter->i < p->gindex)
      return p->games[giter->i++];
  }
  return NO_GAME;
}

static unsigned
gc_games(game_t *gp, unsigned max)
{
  unsigned d, s, e;

  d = 0;
  while (d < max && game_weight(gp[d]) > 0.0)
    d += 1;
  if (d == max)
    return d;
  s = d;
  while (1)
  {
    while (s < max && game_weight(gp[s]) <= 0.0)
      s += 1;
    if (s == max)
      return d;
    e = s;
    while (e < max && game_weight(gp[e]) > 0.0)
      e += 1;
    while (s < e)
      gp[d++] = gp[s++];
  }
}

void
player_gc_games(void)
{
  player_t p;

  assert(Players);
  for (p = 0 ; p < Pindex ; p++)
    Players[p].gindex = gc_games(Players[p].games, Players[p].gindex);
}

/*
**  Sorting the indirect vector.
*/

/* rank in descending order. */
static int
pcomp_rank_desc(const void *a, const void *b)
{
  ps_t *pa = Players + (*(player_t *)a);
  ps_t *pb = Players + (*(player_t *)b);
  irank_t arank = (pa->rated ? pa->rank : 0.0);
  irank_t brank = (pb->rated ? pb->rank : 0.0);

  if (arank < brank)
    return 1;
  if (arank > brank)
    return -1;
  if (pa->high - pa->low < pb->high - pb->low)
    return 1;
  if (pa->high - pa->low > pb->high - pb->low)
    return -1;
  return strcasecmp(pa->name, pb->name);
}

/* rank in ascending order. */
static int
pcomp_rank_asc(const void *a, const void *b)
{
  ps_t *pa = Players + (*(player_t *)a);
  ps_t *pb = Players + (*(player_t *)b);
  irank_t arank = (pa->rated ? pa->rank : 0.0);
  irank_t brank = (pb->rated ? pb->rank : 0.0);

  if (arank > brank)
    return 1;
  if (arank < brank)
    return -1;
  if (pa->high - pa->low > pb->high - pb->low)
    return 1;
  if (pa->high - pa->low < pb->high - pb->low)
    return -1;
  return strcasecmp(pa->name, pb->name);
}

void
player_sortbyrank_desc(void)
{
  assert(Pindir && Players);
  qsort(Pindir, Pindindex, sizeof(player_t), pcomp_rank_desc);
}

void
player_sortbyrank_asc(void)
{
  assert(Pindir && Players);
  qsort(Pindir, Pindindex, sizeof(player_t), pcomp_rank_asc);
}

double
player_get_sorted_mean(void)
{
  unsigned s, e = 0;

  assert(Pindir && Players);
  while (e < Pindindex && !Players[Pindir[e]].rated)
    e += 1;
  s = e;
  while (e < Pindindex && Players[Pindir[e]].rated)
    e += 1;
  if ((e - s) % 2)		/* Odd? */
    return Players[Pindir[(s + e)/2]].rank;
  else				/* Even. */
    return (Players[Pindir[(s + e)/2 - 1]].rank +
	    Players[Pindir[(s + e)/2]].rank)/2;
}
