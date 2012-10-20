/* $Id: game.h,v 1.2 2002/01/27 16:02:54 pem Exp $
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

#ifndef _game_h
#define _game_h

#include <stdlib.h>
#include <time.h>
#include "pg.h"

/* Adds a new game. 'advantage' is the winner's advantage. */
extern game_t game_add(player_t winner, player_t loser,
		       double advantage, double weight, time_t date);

/* Return the number of games. */
extern size_t game_count(void);

/*
**  Destructor.
*/
extern void game_set_weight(game_t, double);

/*
** Accessors.
*/
extern player_t game_winner(game_t);
extern player_t game_loser(game_t);
extern double game_advantage(game_t);
extern double game_weight(game_t);
extern time_t game_date(game_t);

#endif /* _game_h */
