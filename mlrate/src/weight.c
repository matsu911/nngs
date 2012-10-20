/* $Id: weight.c,v 1.2 2002/01/27 16:02:57 pem Exp $
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

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "weight.h"

#ifndef DW_MIN
#define DW_MIN 60
#endif

#ifndef DW_MAX
#define DW_MAX 360
#endif

#ifndef DW_MID
#define DW_MID 25
#endif

#ifndef DW_SLOPE
#define DW_SLOPE 3
#endif

/* Global variables to be configurable. */
unsigned Dweight_min = DW_MIN;
unsigned Dweight_max = DW_MAX;
double Dweight_mid = DW_MID;
double Dweight_slope = DW_SLOPE;

/* The base maximum lifetime for (Pettersen's) uniform aging. */
unsigned Pett_max_days = 180;

#define SECS_PER_DAY (24*3600)

/* Age of a game in days. */
static double
age_of_game(time_t date)
{
  static time_t tnow = 0;

  if (!tnow)
#ifdef FIXTIME
    tnow = FIXTIME;
#else
    tnow = time(NULL);
#endif
    
  return floor(difftime(tnow, date)/SECS_PER_DAY);
}

static double
aw(irank_t r)
{
  return Dweight_min + (Dweight_max -
			Dweight_min)*(1 - 1/(1+exp((r - Dweight_mid)/
						   Dweight_slope)));
}

double
weight_game(time_t date,
	    irank_t whiterank, int whasrank, irank_t blackrank, int bhasrank,
	    weight_style_t ws)
{
  int hasage = 0;
  double age = 0.0;

  switch (ws)
  {
  case weight_off:
    return  1.0;
  case weight_diff:
    age = age_of_game(date);
    hasage = 1;
    if (whasrank && bhasrank)
    {
      irank_t r = (whiterank < blackrank ? whiterank : blackrank); /* MIN */

      return (1 - age/aw(r));
    }
    /* Fall through. */
  case weight_unif:
    if (!hasage)
      age = age_of_game(date);
    return (age >= Pett_max_days ?
	    0.0 :
	    1 - age/Pett_max_days);
  }
  return 1.0;
}
