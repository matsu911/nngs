/* $Id: xdbm.h,v 1.2 2002/01/27 16:02:57 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1998-04-19.
**
** Generic dbm interface. Uses ndbm, gdbm, or a directory tree.
** Key is a null terminated string of at most XDBM_KEYMAXLEN-1 characters.
** Lookup is case insensitive.
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

#ifndef _xdbm_h
#define _xdbm_h

#define XDBM_KEYMAXLEN 256

#ifdef GDBM

#include <gdbm.h>
typedef GDBM_FILE xdbm_t;

#else

#ifdef NDBM

#include <ndbm.h>
typedef DBM *xdbm_t;

#else

typedef struct xdbm *xdbm_t;

#endif /* NDBM */

#endif /* GDBM */

extern xdbm_t xdbm_create(char *dbname);

extern xdbm_t xdbm_open(char *dbname, int writing);

extern void xdbm_close(xdbm_t);

extern int xdbm_store(xdbm_t, char *key, void *, size_t);

extern int xdbm_fetch(xdbm_t, char *key, void *, size_t);

extern int xdbm_delete(xdbm_t, char *key);

extern void xdbm_start(xdbm_t);

extern int xdbm_next(xdbm_t, void *, size_t);

#endif /* _xdbm_h */
