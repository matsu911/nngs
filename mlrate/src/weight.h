/* $Id: weight.h,v 1.2 2002/01/27 16:02:57 pem Exp $
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

#ifndef _weight_h
#define _weight_h

#include <time.h>
#include "rank.h"

/* With "differentiated" aging, games age by the function:
**
**       age
**  1 - -----
**       aw
**
**                                               1
** where     aw(r) = min + (max-min) * (1 - ------------ )
**                                               (r-m)/s
**                                          1 + e
**
** 'min' is the minimum lifespan (Dweight_min), 'max' the maximum
** span (Dweight_max), 'r' is the weaker player's rank, 'm' is the
** mid-point (Dweight_mid), and 's' is the "slope" (Dweight_slope).
*/
extern unsigned Dweight_min;	/* Typically 30-200 (default 60) */
extern unsigned Dweight_max;	/* Typically 300-600 (default 330) */
extern double Dweight_mid;	/* Typically 20-30 (default 25) */
extern double Dweight_slope;	/* Typically 2-4 (default 3) */

/* In Pettersen's "uniform" system game age by the function:
**
**  1 - age/Pett_max_days
**
*/
extern unsigned Pett_max_days;	/* 180 at NNGS */


typedef enum
{
  weight_off,			/* No weight. */
  weight_diff,			/* Differentiated over ranks. */
  weight_unif			/* Uniform weight for all ranks. */
} weight_style_t;

extern double
weight_game(time_t date,
	    irank_t whiterank, int whasrank, irank_t blackrank, int bhasrank,
	    weight_style_t);

#endif /* _weight_h */
