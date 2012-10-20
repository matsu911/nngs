/* $Id: hdbm.h,v 1.2 2002/01/27 16:02:54 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12.
**
** Ratings History Database.
**
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

#ifndef _hdbm_h
#define _hdbm_h

#define HDBM_MAX_POINTS 365

#include <time.h>
#include "player.h"

typedef struct
{
  /* These are originally doubles, but stored with 2 decimal precision as
     shorts to save space. */
  short rating, low, high;
  unsigned short day;
} hdbm_point_t;

/* The 'point' buffer is a circular list, where 'next' points to the
** next entry to be used. From the start, all entries are zeroed, so
** an unused slot can be recognized by examining the 'day' part which is
** then 0.
*/
typedef struct
{
  char name[PLAYER_MAX_NAMELEN+1];
  unsigned short next;
  hdbm_point_t point[HDBM_MAX_POINTS];
} hdbm_player_t;


#ifdef HDBM_GDBM
#define GDBM
#else
/* NDBM has a 1024 byte limit, so we can't use it. */
#ifdef NDBM
#undef NDBM
#endif
#endif

/* Dirty tricks with the preprocessor. ;-) */

#define xdbm_t hdbm_t

#define xdbm_create hdbm_create

#define xdbm_open hdbm_open

#define xdbm_close hdbm_close

#define xdbm_store hdbm_store_s

#define xdbm_fetch hdbm_fetch_s

#define xdbm_delete hdbm_delete

#define xdbm_start hdbm_start

#define xdbm_next hdbm_next_s

#include "xdbm.h"

#define hdbm_store(db, p) \
  hdbm_store_s(db, (p)->name, p, sizeof(hdbm_player_t))

#define hdbm_fetch(db, name, p) \
  hdbm_fetch_s(db, name, p, sizeof(hdbm_player_t))

#define hdbm_next(db, p) \
  hdbm_next_s(db, p, sizeof(hdbm_player_t))

#endif /* _hdbm_h */
