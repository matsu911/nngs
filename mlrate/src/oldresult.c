/* $Id: oldresult.c,v 1.2 2002/01/27 16:02:56 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1998-03.
**
** Reader for the old result format.
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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include "result.h"
#include "player.h"
#include "rank.h"
#include "error.h"

#define LINEBUFSIZE 128

struct result
{
  FILE *fp;
  size_t lineno;
  char whitename[PLAYER_MAX_NAMELEN+1], blackname[PLAYER_MAX_NAMELEN+1];
  irank_t whiterank, blackrank;
  float komi;
  unsigned handicap;
  int wrated, brated;
  char winner[2];
  time_t date;
};

result_t
result_create(FILE *fp)
{
  result_t res;

  assert(fp != NULL);

  res = (result_t)malloc(sizeof(struct result));
  if (res != NULL)
  {
    memset(res, 0, sizeof(struct result));
    res->fp = fp;
  }
  return res;
}

void
result_destroy(result_t res)
{
  if (res)
    free(res);
}

static int
mk_result_date(unsigned year, unsigned month, unsigned day, time_t *tp)
{
  struct tm tms;

  if (year > 1900)
    year -= 1900;
  tms.tm_year = year;
  tms.tm_mon = month-1;
  tms.tm_mday = day;
  tms.tm_hour = 0;
  tms.tm_min = tms.tm_sec = 0;
  tms.tm_isdst = 0;
  *tp = mktime(&tms);
  if (*tp == (time_t)(-1))
    return 0;
  return 1;
}

/*
** Returns 0 on success, 1 on format error (continue reading may work),
** or -1 on syntax or I/O error (continue will probably not work).
*/
int
result_read(result_t res)
{
  char line[LINEBUFSIZE];
  char whiterstr[8], blackrstr[8]; /* Rating strings. */
  unsigned month, day, year;
  int errflag = 0;
  FILE *fin = res->fp;

  assert(res != NULL);

  res->lineno += 1;
  if (fgets(line, LINEBUFSIZE, fin) == NULL)
    return -1;
  if (sscanf(line, "%s %s %s %s %u %f %s %u/%u/%u",
	     res->whitename, whiterstr, res->blackname, blackrstr,
	     &res->handicap, &res->komi, res->winner,
	     &month, &day, &year) != 10)
  {
    error("Bad format at line %lu\n", res->lineno);
    return -1;
  }
  if (!strcmp(whiterstr, "--"))
    res->wrated = 0;
  else
    if (strtoirank(whiterstr, &res->whiterank))
      res->wrated = 1;
    else
    {
      error("Bad rank at line %lu: \"%s\"\n", res->lineno, whiterstr);
      errflag = 1;
    }
  if (!strcmp(blackrstr, "--"))
    res->brated = 0;
  else
    if (strtoirank(blackrstr, &res->blackrank))
      res->brated = 1;
    else
    {
      error("Bad rank at line %lu: \"%s\"\n", res->lineno, blackrstr);
      errflag = 1;
    }
  if (res->handicap > 9)
  {
    error("Handicap larger than 9 at line %lu: %u\n",
	  res->lineno, res->handicap);
    errflag = 1;
  }
  if (res->komi < -100.0 || 100.0 < res->komi)
  {
    error("Bizarre komi at line %lu: %g\n",
	  res->lineno, res->komi);
    errflag = 1;
  }
  else
    if (res->komi < -10.0 || 10.0 < res->komi)
      warning("Unusual komi at line %lu: %g\n", res->lineno, res->komi);
  if (res->winner[0] != 'W' && res->winner[0] != 'B')
  {
    error("Bad winner at line %lu: %s\n", res->lineno, res->winner);
    errflag = 1;
  }
  if (!mk_result_date(year, month, day, &res->date))
  {
    error("Bad date at line %lu: %u-%u-%u\n", res->lineno, year, month, day);
    errflag = 1;
  }
  return errflag;
}

#define RES_ACC(T,F) \
T \
result_##F (result_t res) \
{ \
  assert(res != NULL); \
  return res->F; \
}

RES_ACC(char *, whitename)

RES_ACC(char *, blackname)

RES_ACC(irank_t, whiterank)

RES_ACC(irank_t, blackrank)

RES_ACC(int, wrated)

RES_ACC(int, brated)

RES_ACC(float, komi)

RES_ACC(unsigned, handicap)

RES_ACC(time_t, date)

char
result_winner(result_t res)
{
  assert(res != NULL);
  return res->winner[0];
}

#undef RES_ACC
