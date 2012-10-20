/* $Id: anchor.c,v 1.3 2003/05/05 13:27:59 pem Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include "rank.h"
#include "rdbm.h"
#include "cmd.h"
#include "error.h"

#define USAGE "Usage: [-u] %s <initfile>\n"

#ifndef MAX_ANCHORS
#define MAX_ANCHORS 256
#endif

#define SMALLBUFSIZE 256

typedef struct
{
  char name[RDBM_NAMEMAX];
  double rating;
} anchor_t;


static int
read_name(char *buf, size_t bufsize, FILE *fin, size_t lineno)
{
  int c;
  unsigned i = 0;

  c = getc(fin);
  if (c == EOF)
    return 0;
  if (c != '"')
    errex("Name doesn't start with \" at line %lu\n", lineno);

  while ((c = getc(fin)) != EOF)
    switch (c)
    {
    case '"':
      if (i < bufsize)
	buf[i] = '\0';
      else
	errex("Buffer overflow when reading name at line %lu\n", lineno);
      return 1;
    case '\\':
      c = getc(fin);
      if (c == EOF)
	errex("Unexpected end-of-file at line %lu when reading name\n",
	      lineno);
      /* Fall through. */
    default:
      if (i < bufsize-1)
	buf[i++] = c;
      else
	errex("Buffer overflow when reading name at line %lu\n", lineno);
    }
  errex("Unexpected end-of-file at line %lu when reading name\n", lineno);
  return 0;
}

typedef enum
{
  c_database = 0, c_svalfile, c_ratemin, c_ratemax,
  c_range, c_wgames,  c_winlossq, c_lastplayed
} c_type;

static cmd_tab_t Cmdtab[] =
{
  { "database", c_database, cmd_string },
  { "svalfile", c_svalfile, cmd_string },
  { "ratemin", c_ratemin, cmd_double },
  { "ratemax", c_ratemax, cmd_double },
  { "range", c_range, cmd_double },
  { "wgames", c_wgames, cmd_double },
  { "winlossq", c_winlossq, cmd_double },
  { "lastplayed", c_lastplayed, cmd_unsigned },
  { NULL, 0, 0 }
};

int
main(int argc, char **argv)
{
  char *database = NULL, *svalfile = NULL;
  char buf[SMALLBUFSIZE];
  char *p;
  int c;
  rdbm_t db;
  rdbm_player_t rp;
  unsigned long before;
  anchor_t anchors[MAX_ANCHORS];
  unsigned anchcount;
  time_t now;
  double adjust = 0.0;

  double ratemin = 31.0, ratemax = 34.0;
  double range = 0.5;
  double wgames = 20.0;
  double winlossq = 0.25;
  unsigned lastplayed = 14;
  int no_sval_update = 0;

  error_init(argv);

  if (argc > 1)
  {
    if (strcmp(argv[1], "-u") == 0)
    {
      no_sval_update = 1;
      argc -= 1;
      argv += 1;
    }
  }
  if (argc != 2)
    errex(USAGE, error_progname());

  {
    FILE *cfile;
    unsigned line = 0;
    cmd_t arg;
    int x;

    if ((cfile = fopen(argv[1], "r")) == NULL)
      perrex("\"%s\" (for reading)\n", argv[1]);
    x = 1;
    while (x)
    {
      int ret = cmd_read(Cmdtab, &arg, cfile, &line);

      switch (ret)
      {
      case c_database:
	database = arg.arg.s;
	break;
      case c_svalfile:
	svalfile = arg.arg.s;
	break;
      case c_ratemin:
	ratemin = arg.arg.d;
	break;
      case c_ratemax:
	ratemax = arg.arg.d;
	break;
      case c_range:
	range = arg.arg.d;
	break;
      case c_wgames:
	wgames = arg.arg.d;
	break;
      case c_winlossq:
	winlossq = arg.arg.d;
	break;
      case c_lastplayed:
	lastplayed = arg.arg.u;
	break;
      default:
	switch (arg.err)
	{
	case cmd_err_cmd:
	  errex("Unknown command on line %u in file \"%s\"\n", line, argv[1]);
	case cmd_err_narg:
	  errex("Missing argument on line %u in file \"%s\"\n", line, argv[1]);
	case cmd_err_type:
	  errex("Wrong type of argument on line %u in file \"%s\"\n",
		line, argv[1]);
	case cmd_err_xarg:
	  errex("Extraneous argument on line %u in file \"%s\"\n",
		line, argv[1]);
	case cmd_err_tab:
	  errex("Strange type in command table in file \"%s\"\n", argv[1]);
	case cmd_err_mem:
	  errex("malloc() failed when parsin file \"%s\"\n", argv[1]);
	default:
	  x = 0;
	  break;
	}
      }
    }
    fclose(cfile);
  }

  if (database == NULL)
    errex("Missing command in \"%s\": database\n", argv[1]);

  if (fgets(buf, SMALLBUFSIZE, stdin) == NULL)
    errex("Empty input\n");
  before = strtoul(buf, &p, 10);
  if (p == buf)
    errex("Bad timestamp: %s\n", buf);
  /* Temporary fix. The precision of 'lastplayed' in the player DB is only
     a day, so we truncate. */
  before /= (24 * 3600);
  before *= (24 * 3600);

  anchcount = 0;
  while (anchcount < MAX_ANCHORS && !feof(stdin))
  {
    if (!read_name(anchors[anchcount].name, RDBM_NAMEMAX, stdin,
		   anchcount+1))
      break;
    if (fscanf(stdin, "%lf", &anchors[anchcount].rating) != 1)
      errex("Bad, or missing, rating number at line %lu\n", anchcount+1);
    while ((c = getc(stdin)) != EOF && c != '\n')
      ;
    if (c == EOF)
      break;
    anchcount += 1;
  }
  if (anchcount == MAX_ANCHORS)
    warning("Maximum of %u anchors read\n", MAX_ANCHORS);

  if (anchcount > 0)
  {
    double average = 0.0, deviation = 0.0;
    unsigned i, count = 0;

    if ((db = rdbm_open(database, 0)) == NULL)
      perrex("\"%s\"\n", database);
    for (i = 0 ; i < anchcount ; i++)
    {
      if (rdbm_fetch(db, anchors[i].name, &rp))
      {
	if (difftime(rp.lastplayed, before) >= 0.0)
	  continue;
	if (rp.rank[0] && rp.rating > 0.0)
	{
	  double diff = rp.rating - anchors[i].rating;

	  deviation += diff*diff;
	  average += diff;
	  count += 1;
	  fprintf(stderr, "%-*s  %7.3f\n", RDBM_NAMEMAX, rp.name, diff);
	}
      }
    }
    rdbm_close(db);
    if (count > 0)
      average /= count;
    fprintf(stderr,   "Average:   %7.3f\n", average);
    if (count > 1)
      fprintf(stderr, "Deviation: %7.3f\n", sqrt(deviation/(count-1)));

    if (! no_sval_update)
    {
      FILE *fp;
      double sval;

      if (svalfile == NULL)
	errex("Missing command in \"%s\": svalfile\n", argv[1]);
      if ((fp = fopen(svalfile, "r")) == NULL)
	perrex("\"%s\" (for reading)\n", svalfile);
      if (fgets(buf, SMALLBUFSIZE, fp) == NULL)
	errex("Empty file: \"%s\"\n", svalfile);
      sval = strtod(buf, &p);
      if (p == buf)
	errex("Bad floating point number in \"%s\": %s\n", svalfile, buf);
      fclose(fp);

      if (average <= -0.01 || 0.01 <= average)
      {
	adjust = average;		/* Reset for the adjustment below. */
	fprintf(stderr, "Old start value %7.3f, new %7.3f\n",
		sval, sval-average);
	sval -= average;
	if ((fp = fopen(svalfile, "w")) == NULL)
	  perrex("\"%s\" (for reading)\n", svalfile);
	fprintf(fp, "%.3f\n", sval);
	fclose(fp);
      }
    }
  }

  now = time(NULL);
  printf("%lu\n", now);

  /* Select and print the new set of anchors */
  if ((db = rdbm_open(database, 0)) == NULL)
    perrex("\"%s\"\n", database);
  rdbm_start(db);
  while (rdbm_next(db, &rp))
  {
    char *p;
    double q, s;

    if (rp.rank[0] == '\0')
      continue;
    if (rp.rating < ratemin || ratemax <= rp.rating)
      continue;
    if (rp.high - rp.low > range)
      continue;
    s = rp.wwins + rp.wlosses;
    if (s < wgames)
      continue;
    /* Smallest / biggest */
    q = (rp.wwins > rp.wlosses ? rp.wlosses/rp.wwins : rp.wwins/rp.wlosses);
    if (q < winlossq)
      continue;
    if (difftime(now, rp.lastplayed) > (double)(lastplayed * 24*3600UL))
      continue;

    putchar('"');
    for (p = rp.name ; *p ; p++)
    {
      if (*p ==  '"' || *p == '\\')
	putchar('\\');
      putchar(*p);
    }
    printf("\"\t%5.3f\n", rp.rating - adjust);
  }
  rdbm_close(db);

  exit(0);
}
