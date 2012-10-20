/* $Id: rank.h,v 1.3 2002/01/27 18:03:01 pem Exp $
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

#ifndef _rank_h
#define _rank_h

/* Lowest kyu and highest dan we allow. */
#define KYU_FLOOR          30
#define DAN_CEILING         7

/* Kyu ranks are in [0,30[.
** Dan ranks are in [30,37[
*/
#define IRANK_DAN_ICEILING 36.0
#define IRANK_DAN_ILIMIT   (IRANK_DAN_ICEILING - 6.0)

/* These are use to set the span for the equation solver. */
#define RANK_MINIMUM    0.0
#define RANK_MAXIMUM   50.0

/* Returns the difference as a double float. */
#define RANK_DIFF(X,Y)  ((X) - (Y))

/* Internal rank representation is a float, but don't count on it
** being a double in particular.
*/
typedef double irank_t;

/* Converts a string to internal rank.
** Returns 1 if successful, 0 if a bad rank string.
*/
extern int strtoirank(char *s, irank_t *rp);

/* Converts internal rank to string. */
extern void iranktostr(irank_t r, char *s, unsigned smax);

extern irank_t blacks_advantage(double komi, unsigned hcap);

/* For debugging of blacks_advantage(). */
extern void rank_diff_print(void);

#endif /* _rank_h */
