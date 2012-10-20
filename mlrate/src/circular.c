/* $Id: circular.c,v 1.2 2002/09/19 07:11:42 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1999-06-21
**
**   Copyright (C) 1999-2002  Per-Erik Martin
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
#include <stdio.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include "circular.h"
#include "error.h"

/* Must be even, preferably not too large (4-16). */
#define HISTORY_LENGTH 8

void
circular_check(player_t p)
{
  static player_t History[HISTORY_LENGTH];
  static size_t Hist_idx = 0;

  if (Hist_idx < HISTORY_LENGTH)
    History[Hist_idx++] = p;	/* Add */
  else
  {
    int x = 0;
    size_t i;

    /* Shift */
    for (i = 0 ; i < HISTORY_LENGTH-1 ; i++)
      History[i] = History[i+1];
    History[i] = p;		/* Add last */

    /* Check for the pattern a b a b a b a b... */
    if (History[0] != History[1])
      for (i = 0 ; i < HISTORY_LENGTH-2 ; i++)
      {
	p = (i % 2 == 0 ? History[0] : History[1]);
	if (History[i] == History[i+2] &&
	    History[i] == p)
	  x = 1;
	else
	  x = 0;
      }

    if (x)
    {
      warning("Circular dependency between \"%s\" and \"%s\"\n",
	      player_get_name(History[0]), player_get_name(History[1]));
      /* Pick the one with the fewest number of rated games */
      if (player_get_ratedgames(History[0]) < player_get_ratedgames(History[1]))
	p = History[0];
      else
	p = History[1];
      warning("Dropping \"%s\"\n", player_get_name(p));
      player_set_rated(p, 0);
      Hist_idx = 0;
    }
  }
}
