/* $Id: t2res.c,v 1.4 2002/01/27 17:56:53 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1996-04-10, 1998-12.
**
** Reads this format from standard input, with space or tabs as
** delimiters:
**
** 1 Vesa Laatikainen    5d   5+   6+   7+   2-   8+   3+
** 2 Matti Siivola       5d   4+  10+  11+   1+   3-   7+
** 3 Magnus Persson      2d  11+   7-  12+  10+   2+   1-
** 4 Magnus Kylemark     2d   2-   9+  10-  12+   6+  11+
**
** That is, typical MacMahon tournament results with points, country,
** SOS, etc, removed. Free rounds must be marked with 0. (Thus, a player's
** number must not be 0.)
** There's a limit on the length of a line (currently 1024 byte) which
** should be enough for most tournaments...
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
#include <unistd.h>
#include <string.h>
#include "error.h"

#define INITSIZE     32
#define BUFSIZE    1024
#define DELIM    " \t\n\r"

typedef struct
{
  unsigned long num;		/* 0 if an unused slot. */
  int sum;
  char *name;
  char rank[4];
  size_t len, max;
  long *results;		/* Positive for a win, negative for loss. */
} p_t;

int
main(int argc, char **argv)
{
  char name[BUFSIZE], buf[BUFSIZE];
  char dates[64];
  p_t *players;
  size_t i, count, pmax = INITSIZE;
  int c;
  
  error_init(argv);

  dates[0] = '\0';
  opterr = 0;
  while ((c = getopt(argc, argv, "d:h?")) != EOF)
    switch (c)
    {
    case 'd':
      strncpy(dates, optarg, 64);
      dates[63] = '\0';
      break;
    case 'h':
    case '?':
    default:
      errex("Usage: %s -d <date>\n", error_progname());
    }

  if (dates[0] == '\0')
    errex("Usage: %s -d <date>\n", error_progname());

  players = (p_t *)malloc(INITSIZE * sizeof(p_t));
  if (!players)
    perrex("malloc(%lu) failed\n", INITSIZE * sizeof(p_t));
  for (i = 0 ; i < pmax ; i++)
    players[i].num = 0;

  count = 0;
  while (!feof(stdin) && fgets(buf, BUFSIZE, stdin))
  {
    unsigned long n;
    char *s, *e;
    p_t *p;

    count += 1;
    buf[BUFSIZE-1] = '\0';
    s = strtok(buf, DELIM);
    if (!s)
      continue;
    if (*s == '#')
      continue;
    n = strtoul(s, &e, 10);
    if (s == e)
      perrex("Bad number at line %d: %s\n", count, s);
    if (n >= pmax)
    {
      size_t omax = pmax;

      while (pmax <= n)
	pmax *= 2;
      players = (p_t *)realloc(players, pmax * sizeof(p_t));
      if (!players)
	perrex("realloc(players, %lu) failed\n", pmax * sizeof(p_t));
      while (omax < pmax)
	players[omax++].num = 0;
    }
    p = players + n;
    p->num = n;

    name[0] = '\0';
    n = 0;
    do {
      s = strtok(NULL, DELIM);
      if (!s)
	perrex("End of line while scanning name at line %lu\n", count);
      n = strtoul(s, &e, 10);
      if (n)
	switch (*e)		/* Maybe a rank. */
	{
	case 'D': case 'd':
	case 'K': case 'k':
	  strncpy(p->rank, s, 4);
	  p->rank[3] = '\0';
	  break;
	default:
	  strcpy(p->rank, "--");
	  n = 0;
	}
      if (!n)
      {				/* Not a rank. */
	char *np = name+strlen(name);

	if (name[0])
	  *np++ = ' ';
	while (*s)
	{
	  if (*s == '"' || *s == '\\')
	    *np++ = '\\';
	  *np++ = *s++;
	}
	*np = '\0';
      }
    } while (n == 0);

    p->name = strdup(name);
    if (!p->name)
      perrex("strdup(\"%s\") failed\n", name);

    p->max = INITSIZE;
    p->results = (long *)malloc(INITSIZE * sizeof(long));
    if (!p->results)
      perrex("malloc(%lu) failed\n", INITSIZE * sizeof(long));

    i = 0;
    while ((s = strtok(NULL, DELIM)) && *s)
    {
      long r = strtol(s, &e, 10);

      if (s == e)
	perrex("Bad result field at line %lu: %s\n", count, s);
      if (*e == '-' && r > 0)
	p->results[i] = -r;
      else
	p->results[i] = r;
      i += 1;
      if (i == p->max)
      {
	p->max *= 2;
	p->results = (long *)realloc(p->results, p->max * sizeof(long));
	if (!p->results)
	  perrex("realloc(p->results, %ul) failed\n", p->max * sizeof(long));
      }
    }
    p->len = i;
    p->sum = 0;
  }

  for (i = 0 ; i < pmax ; i++)
  {
    p_t *p = players+i;

    if (p->num)
    {
      size_t j;

      for (j = 0 ; j < p->len ; j++)
	if (p->results[j] != 0)
	{
	  short win;
	  long opn;
	  p_t *opp;

	  if (p->results[j] > 0)
	  {
	    win = 1;
	    opn = p->results[j];
	  }
	  else
	  {
	    win = 0;
	    opn = -(p->results[j]);
	  }
	  if (opn >= pmax)
	  {
	    warning("%s's result %lu out of bounds: %ld\n",
		    p->name, j+1, opn);
	    continue;
	  }
	  opp = players + opn;
	  if (opp->num == 0)
	  {
	    warning("%s's result %lu against no-one: %ld\n",
		    p->name, j+1, opn);
	    continue;
	  }
	  if (win && p->num != -(opp->results[j]))
	  {
	    warning("Table inconsistency in round %lu between %lu and %lu\n",
		    j+1, p->num, opp->num);
	    continue;
	  }
	  if (!win && p->num != opp->results[j])
	  {
	    warning("Table inconsistency in round %lu between %lu and %lu\n",
		    j+1, p->num, opp->num);
	    continue;
	  }
	  if (rand() % 2)
	    printf("\"%s\" %s \"%s\" %s 0 5.5 %c %s\n",
		   p->name, p->rank, opp->name, opp->rank,
		   (win ? 'W' : 'B'), dates);
	  else
	    printf("\"%s\" %s \"%s\" %s 0 5.5 %c %s\n",
		   opp->name, opp->rank, p->name, p->rank,
		   (win ? 'B' : 'W'), dates);
	  opp->results[j] = 0;
	}
    }
  }
  exit(0);
}
