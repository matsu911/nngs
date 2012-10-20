/* $Id: result.c,v 1.2 2002/01/27 16:02:57 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1998-03.
**
** Reader for the new result format.
** The major difference is that the names are quoted with " so
** we can allow any characters in name (esp. blanks).
** In a name, backslash (\) quotes whatever is following it, so
** a \" is a ", \\ is a \, and (pointlessly) \a is an a.
** The other difference is that unknown rank is marked with -, and
** we use ISO dates.
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

#define BUFSIZE 16

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

static int
skip_whitespace(int c, FILE *fin)
{
  while (c == ' ' || c == '\t')
    c = getc(fin);
  return c;
}

static int
read_name(int c, char *buf, size_t bufsize, FILE *fin, size_t lineno)
{
  unsigned i = 0;

  if (c != '"')
    error("Name doesn't start with \" at line %lu\n", lineno);
  while ((c = getc(fin)) != EOF)
    switch (c)
    {
    case '"':
      if (i < bufsize)
	buf[i] = '\0';
      else
      {
	error("Buffer overflow when reading name at line %lu\n", lineno);
	return EOF;
      }
      return getc(fin);
    case '\\':
      c = getc(fin);
      if (c == EOF)
      {
	error("Unexpected end-of-file at line %lu when reading name\n",
	      lineno);
	return EOF;
      }
      /* Fall through. */
    default:
      if (i < bufsize-1)
	buf[i++] = c;
      else
      {
	error("Buffer overflow when reading name at line %lu\n", lineno);
	return EOF;
      }
    }
  error("Unexpected end-of-file at line %lu when reading name\n", lineno);
  return EOF;
}

static int
read_token(int c, char *buf, size_t bufsize, FILE *fin, size_t lineno)
{
  unsigned i = 0;

  do {
    switch (c)
    {
    case ' ': case '\t': case '\n':
      if (i < bufsize)
	buf[i] = '\0';
      else
      {
	error("Unexpected end-of-file at line %lu when reading token\n",
	      lineno);
	return EOF;
      }
      return c;
    default:
      if (i < bufsize-1)
	buf[i++] = c;
      else
      {
	error("Unexpected end-of-file at line %lu when reading token\n",
	      lineno);
	return EOF;
      }
    }
  } while ((c = getc(fin)) != EOF);
  error("Unexpected end-of-file at line %lu when reading token\n", lineno);
  return EOF;
}

/*
** Returns 0 on success, 1 on format error (continue reading may work),
** or -1 on EOF, or syntax or I/O error (continue will probably not work).
*/
int
result_read(result_t res)
{
  char buf[BUFSIZE];
  int c;
  char *p;
  FILE *fin = res->fp;

  assert(res != NULL);

  res->lineno += 1;
  if ((c = skip_whitespace(getc(fin), fin)) == EOF)
    goto err;

  /* White's name */
  if ((c = read_name(c, res->whitename, PLAYER_MAX_NAMELEN+1,
		     fin, res->lineno)) == EOF)
    goto err;

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* White's rank */
  if ((c = read_token(c, buf, BUFSIZE,
		      fin, res->lineno)) == EOF)
    goto err;
  if (!strcmp(buf, "-"))
    res->wrated = 0;
  else
    if (strtoirank(buf, &res->whiterank))
      res->wrated = 1;
    else
    {
      warning("Bad rank at line %lu: \"%s\"\n", res->lineno, buf);
      res->wrated = 0;
    }

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* Black's name */
  if ((c = read_name(c, res->blackname, PLAYER_MAX_NAMELEN+1,
		     fin, res->lineno)) == EOF)
    goto err;

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* Black's rank */
  if ((c = read_token(c, buf, BUFSIZE,
		      fin, res->lineno)) == EOF)
    goto err;
  if (!strcmp(buf, "-"))
    res->brated = 0;
  else
    if (strtoirank(buf, &res->blackrank))
      res->brated = 1;
    else
    {
      warning("Bad rank at line %lu: \"%s\"\n", res->lineno, buf);
      res->brated = 0;
    }

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* Handicap */
  if ((c = read_token(c, buf, BUFSIZE,
		      fin, res->lineno)) == EOF)
    goto err;
  res->handicap = strtoul(buf, &p, 10);
  if (p == buf || *p != '\0')
  {
    error("Bad handicap at line %lu: %s\n", res->lineno, buf);
    goto skip;
  }
  if (res->handicap > 9)
    warning("Handicap larger than 9 at line %lu: %u\n",
	    res->lineno, res->handicap);

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* Komi */
  if ((c = read_token(c, buf, BUFSIZE,
		      fin, res->lineno)) == EOF)
    goto err;
  res->komi = strtod(buf, &p);
  if (p == buf || *p != '\0')
  {
    error("Bad komi at line %lu: %s\n", res->lineno, buf);
    goto skip;
  }
  if (res->komi < -100.0 || 100.0 < res->komi)
  {
    error("Bizarre komi at line %lu: %g\n",
	  res->lineno, res->komi);
    goto skip;
  }
  else
    if (res->komi < -10.0 || 10.0 < res->komi)
      warning("Unusual komi at line %lu: %g\n", res->lineno, res->komi);

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* Winner */
  if ((c = read_token(c, buf, BUFSIZE,
		      fin, res->lineno)) == EOF)
    goto err;
  if ((buf[0] != 'B' && buf[0] != 'W') || buf[1] != '\0')
  {
    error("Bad winner at line %lu: %s\n", res->lineno, buf);
    goto skip;
  }
  res->winner[0] = buf[0]; res->winner[1] = '\0';

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;

  /* Date: YYYY-MM-DD */
  if ((c = read_token(c, buf, BUFSIZE,
		      fin, res->lineno)) == EOF)
    goto err;
  {
    unsigned year, month, day;
    int x = 0;

    year = strtoul(buf, &p, 10);
    if (p == buf || *p != '-')
      x = 1;
    month = strtoul(buf+5, &p, 10);
    if (p == buf+5 || *p != '-')
      x = 1;
    day = strtoul(buf+8, &p, 10);
    if (p == buf+8 || *p != '\0')
      x = 1;
    if (x || !mk_result_date(year, month, day, &res->date))
    {
      error("Bad date at line %lu: %s\n", res->lineno, buf);
      goto skip;
    }
  }

  if ((c = skip_whitespace(c, fin)) == EOF)
    goto err;
  if (c != '\n')
  {
    warning("Unexpected character at line %lu: '%c'\n", res->lineno, c);
    goto skip;
  }

  return 0;

skip:
  while (c != '\n' && c != EOF)
    c = getc(fin);
  return 1;
err:
  return -1;
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
