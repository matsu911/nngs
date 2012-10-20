/* $Id: rdbm.h,v 1.2 2002/01/27 16:02:56 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12.
**
** Ratings database.
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

#ifndef _rdbm_h
#define _rdbm_h

#ifndef RDBM_NAMEMAX
#define RDBM_NAMEMAX 31
#endif

#include <time.h>

typedef struct
{
  char name[RDBM_NAMEMAX+1];
  char rank[4];
  int star;			/* 1 or 0 */
  double rating, error, low, high;
  unsigned wins, losses;
  double wwins, wlosses;	/* Weighted sums. */
  time_t lastplayed;
} rdbm_player_t;

#include <stdio.h>

void
rdbm_printheader(FILE *fp);

void
rdbm_printline(FILE *fp, rdbm_player_t *rp);

#ifdef RDBM_GDBM
#define GDBM
#else
#ifdef RDBM_NDBM
#define NDBM
#endif
#endif

/* Dirty tricks with the preprocessor. ;-) */

#define xdbm_t rdbm_t

#define xdbm_create rdbm_create

#define xdbm_open rdbm_open

#define xdbm_close rdbm_close

#define xdbm_store rdbm_store_s

#define xdbm_fetch rdbm_fetch_s

#define xdbm_delete rdbm_delete

#define xdbm_start rdbm_start

#define xdbm_next rdbm_next_s

#include "xdbm.h"

#define rdbm_store(db, p) \
  rdbm_store_s(db, (p)->name, p, sizeof(rdbm_player_t))

#define rdbm_fetch(db, name, p) \
  rdbm_fetch_s(db, name, p, sizeof(rdbm_player_t))

#define rdbm_next(db, p) \
  rdbm_next_s(db, p, sizeof(rdbm_player_t))

#endif /* _rdbm_h */
