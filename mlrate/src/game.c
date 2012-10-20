/* $Id: game.c,v 1.2 2002/01/27 16:02:54 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12-12
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

/* A safety. Regard games with weight less or equal to this
** as having weight 0.
*/
#ifndef WEIGHT_ZERO
#define WEIGHT_ZERO 0.001
#endif

#define GAMES_INIT_SIZE 16384L

typedef struct
{
  player_t winner, loser;
  double advantage, weight;
  time_t date;
} gs_t;

static gs_t *Games = NULL;
static size_t Gsize = 0, Gindex = 0;

game_t
game_add(player_t winner, player_t loser, double advantage, double weight,
	 time_t date)
{
  if (Gindex == Gsize)
  {
    Gsize = (Gsize == 0 ? GAMES_INIT_SIZE : 2 * Gsize);
    Games = (gs_t *)realloc(Games, Gsize * sizeof(gs_t));
    if (!Games)
      errex("realloc(...,%lu) failed\n", Gsize * sizeof(gs_t));
  }
  Games[Gindex].winner = winner;
  Games[Gindex].loser = loser;
  Games[Gindex].advantage = advantage;
  Games[Gindex].weight = (weight < WEIGHT_ZERO ? 0.0 : weight);
  Games[Gindex].date = date;
  Gindex += 1;
  player_game_add(winner, Gindex-1);
  player_game_add(loser, Gindex-1);
  return Gindex;
}

size_t
game_count(void)
{
  return Gindex;
}

void
game_set_weight(game_t g, double w)
{
  assert(Games && 0 <= g && g < Gindex);
  if (w < WEIGHT_ZERO)
    w = 0.0;
  Games[g].weight = w;
}

#define GGET(Type,Field) \
Type \
game_ ## Field (game_t g) \
{ \
  assert(Games && 0 <= g && g < Gindex); \
  return Games[g].Field; \
}

GGET(player_t,winner)
GGET(player_t,loser)
GGET(double,advantage)
GGET(double,weight)
GGET(time_t,date)

#undef GGET
