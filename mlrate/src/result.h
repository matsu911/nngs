/* $Id: result.h,v 1.2 2002/01/27 16:02:57 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1998-03.
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

#ifndef _readres_h
#define _readres_h

#include <stdio.h>
#include <time.h>
#include "rank.h"

typedef struct result *result_t;

/*
** Create/free object.
*/
extern result_t result_create(FILE *);

extern void result_destroy(result_t);

/*
** Reads a result line.
**
** Returns 0 on success, 1 on format error (continue should work),
** or -1 on EOF, syntax or I/O error (continue will probably not work).
**
*/
extern int result_read(result_t);

/*
** Accessors
*/
extern char *result_whitename(result_t);

extern char *result_blackname(result_t);

extern irank_t result_whiterank(result_t);

extern irank_t result_blackrank(result_t);

extern int result_wrated(result_t);

extern int result_brated(result_t);

extern float result_komi(result_t);

extern unsigned result_handicap(result_t);

/* Returns the character 'W' or 'B'. */
extern char result_winner(result_t);

extern time_t result_date(result_t);

#endif /* _readres_h */
