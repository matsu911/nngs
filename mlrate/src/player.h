/* $Id: player.h,v 1.2 2002/01/27 16:02:56 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12.
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

#ifndef _player_h
#define _player_h

#include <stdlib.h>
#include <time.h>
#include "pg.h"
#include "rank.h"

#ifndef PLAYER_MAX_NAMELEN
#define PLAYER_MAX_NAMELEN 32
#endif

typedef size_t piter_t;

typedef struct
{
  size_t i;
  player_t p;
} giter_t;


/*
**  Creators.
*/

extern player_t player_add(char *name, irank_t rank, int rated, int ignore);

extern void player_game_add(player_t, game_t);

/*
**  Accessors.
*/

extern int player_get(char *name, player_t *);

extern size_t player_count(void);

extern char *player_get_name(player_t);
extern irank_t player_get_rank(player_t);
extern int player_get_rated(player_t);
extern int player_get_ignore(player_t);
extern unsigned player_get_ratedgames(player_t);
extern unsigned player_get_wins(player_t);
extern unsigned player_get_losses(player_t);
extern double player_get_wwins(player_t);
extern double player_get_wlosses(player_t);
extern double player_get_rerror(player_t);
extern double player_get_low(player_t);
extern double player_get_high(player_t);
extern time_t player_get_lastplayed(player_t);


/*
**  Destructors.
*/

extern void player_set_rank(player_t, irank_t);
extern void player_set_rated(player_t, int);
extern void player_set_ignore(player_t, int);
extern void player_set_ratedgames(player_t, unsigned wins, unsigned losses);
extern void player_set_wratedgames(player_t, double wins, double losses);
extern void player_set_wins(player_t, unsigned);
extern void player_set_losses(player_t, unsigned);
extern void player_set_wwins(player_t, double);
extern void player_set_wlosses(player_t, double);
extern void player_set_rerror(player_t, double);
extern void player_set_low(player_t, double);
extern void player_set_high(player_t, double);
extern void player_set_lastplayed(player_t, time_t);

extern void player_gc_games(void);

/*
**  Iterators.
*/

extern void player_start(piter_t *);
extern player_t player_next(piter_t *);

extern void player_games_start(player_t, giter_t *);
extern game_t player_games_next(giter_t *);

extern void player_sortbyrank_desc(void);
extern void player_sortbyrank_asc(void);

extern double player_get_sorted_mean(void);

#endif /* _player_h */
