/* $Id: mlrate.c,v 1.6 2002/06/24 14:47:13 pem Exp $
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "mlr.h"
#include "player.h"
#include "game.h"
#include "rank.h"
#include "weight.h"
#include "error.h"
#include "rdbm.h"
#include "result.h"
#include "cmd.h"

#ifndef DEFAULT_MEAN_START
#define DEFAULT_MEAN_START 26.5
#endif

#ifndef MAX_ERROR_SPAN
#define MAX_RANK_SPAN 4.0
#endif

#ifndef MAX_STAR_SPAN
#define MAX_STAR_SPAN 2.0
#endif

static double Max_rank_span = MAX_RANK_SPAN;
static double Max_star_span = MAX_STAR_SPAN;

#define LINEBUFSIZE 128

static void print_ratings(FILE *fout, char *dbname, FILE *fgraph, FILE *fdiag);

static void timer_start(void);
static void timer_print(FILE *fdiag, char *s);

#if 0
static void read_ratings(FILE *f, FILE *fdiag);
#endif

static void read_ignore(FILE *fign, FILE *fdiag);

#define USAGE "Usage: %s [-v -|<diagfile>] <initfile>\n"

typedef enum
{
  c_results, c_ignore, c_ratings, c_database, c_graph,
  c_svalfile, c_weight, c_dwmid, c_dwmin, c_dwmax, c_dwslope, c_uwdays, c_s,
  c_maxrankspan, c_maxstarspan
} c_type;

static cmd_tab_t Cmdtab[] =
{
  { "results", c_results, cmd_string },
  { "ignore", c_ignore, cmd_string },
  { "ratings", c_ratings, cmd_string },
  { "database", c_database, cmd_string },
  { "graph", c_graph, cmd_string },
  { "svalfile", c_svalfile, cmd_string },
  { "weight", c_weight, cmd_string },
  { "dwmid", c_dwmid, cmd_double },
  { "dwmin", c_dwmin, cmd_unsigned },
  { "dwmax", c_dwmax, cmd_unsigned },
  { "dwslope", c_dwslope, cmd_double },
  { "uwdays", c_uwdays, cmd_unsigned },
  { "S", c_s, cmd_double },
  { "maxrankspan", c_maxrankspan, cmd_double },
  { "maxstarspan", c_maxstarspan, cmd_double },
  { NULL, 0, 0 }
};

int
main(int argc, char **argv)
{
  int c;
  double meanstart = DEFAULT_MEAN_START;
  FILE *fin = stdin, *fout = stdout, *fdiag = NULL, *fgraph = stderr,
    *fign = NULL, *fms = NULL;
  char *dbname = NULL;
  weight_style_t ws = weight_diff;

  error_init(argv);

  opterr = 0;
  while ((c = getopt(argc, argv, "v:h?")) != EOF)
    switch (c)
    {
    case 'v':
      if (!strcmp("-", optarg))
	fdiag = stderr;
      else
	if ((fdiag = fopen(optarg, "w")) == NULL)
	  perrex("\"%s\" (for writing)\n", optarg);
      break;
    case '?': case 'h':
    default:
      errex(USAGE, error_progname());
    }

  if (argv[optind] == NULL)
    errex("Missing initfile argument\n");

  {
    FILE *cfile;
    unsigned line = 0;
    cmd_t arg;
    int x;

    if ((cfile = fopen(argv[optind], "r")) == NULL)
      perrex("\"%s\" (for reading)\n", argv[optind]);
    x = 1;
    while (x)
    {
      int ret = cmd_read(Cmdtab, &arg, cfile, &line);

      switch (ret)
      {
      case c_results:
	if ((fin = fopen(arg.arg.s, "r")) == NULL)
	  perrex("\"%s\" (for reading), at line %u in \"%s\"\n",
		 arg.arg.s, line, argv[optind]);
	free(arg.arg.s);
	break;
      case c_ignore:
	if ((fign = fopen(arg.arg.s, "r")) == NULL)
	  perrex("\"%s\" (for reading), at line %u in \"%s\"\n",
		 arg.arg.s, line, argv[optind]);
	free(arg.arg.s);
	break;
      case c_ratings:
	if ((fout = fopen(arg.arg.s, "w")) == NULL)
	  perrex("\"%s\" (for writing), at line %u in \"%s\"\n",
		 arg.arg.s, line, argv[optind]);
	free(arg.arg.s);
	break;
      case c_database:
	dbname = arg.arg.s;
	break;
      case c_graph:
	if ((fgraph = fopen(arg.arg.s, "w")) == NULL)
	  perrex("\"%s\" (for writing), at line %u in \"%s\"\n",
		 arg.arg.s, line, argv[optind]);
	free(arg.arg.s);
	break;
      case c_svalfile:
	if ((fms = fopen(arg.arg.s, "r")) == NULL)
	  perrex("\"%s\" (for reading), at line %u in \"%s\"\n",
		 arg.arg.s, line, argv[optind]);
	free(arg.arg.s);
	break;
      case c_weight:
	if (!strcasecmp(arg.arg.s, "off"))
	  ws = weight_off;
	else if (!strcasecmp(arg.arg.s, "diff"))
	  ws = weight_diff;
	else if (!strcasecmp(arg.arg.s, "unif"))
	  ws = weight_unif;
	else
	  errex("Bad argument to 'weight': %s  (off|diff|unif expected)\n",
		arg.arg.s);
	free(arg.arg.s);
	break;
      case c_dwmid:
	Dweight_mid = arg.arg.d;
	break;
      case c_dwmin:
	Dweight_min = arg.arg.u;
	break;
      case c_dwmax:
	Dweight_max = arg.arg.u;
	break;
      case c_dwslope:
	Dweight_slope = arg.arg.d;
	break;
      case c_uwdays:
	Pett_max_days = arg.arg.u;
	break;
      case c_s:
	Mlrate_s = arg.arg.d;
	break;
      case c_maxrankspan:
	Max_rank_span = arg.arg.d;
	break;
      case c_maxstarspan:
	Max_star_span = arg.arg.d;
	break;
      default:
	switch (arg.err)
	{
	case cmd_err_cmd:
	  errex("Unknown command on line %u in file \"%s\"\n",
		line, argv[optind]);
	case cmd_err_narg:
	  errex("Missing argument on line %u in file \"%s\"\n",
		line, argv[optind]);
	case cmd_err_type:
	  errex("Wrong type of argument on line %u in file \"%s\"\n",
		line, argv[optind]);
	case cmd_err_xarg:
	  errex("Extraneous argument on line %u in file \"%s\"\n",
		line, argv[optind]);
	case cmd_err_tab:
	  errex("Strange type in command table in file \"%s\"\n",
		argv[optind]);
	case cmd_err_mem:
	  errex("malloc() failed when parsin file \"%s\"\n",
		argv[optind]);
	default:
	  x = 0;
	  break;
	}
      }
    }
    fclose(cfile);
  }

  timer_start();

  if (fms)
  {
    char buf[64];

    if (fgets(buf, 64, fms))
    {
      char *p;

      meanstart = strtod(buf, &p);
      if (p == buf || *p != '\n')
	errex("Bad number startvalue: %s", buf);
    }
    fclose(fms);
  }

  if (fign)
  {
    read_ignore(fign, fdiag);
    fclose(fign);
  }

  {
    int resflag;
    result_t res = result_create(fin);

    if (res == NULL)
      errex("result_create() failed\n");

    while ((resflag = result_read(res)) > -1)
    {
      if (resflag == 0)
      {
	time_t date = result_date(res);
	double weight = weight_game(date,
				    result_whiterank(res), result_wrated(res),
				    result_blackrank(res), result_brated(res),
				    ws);

	switch (result_winner(res))
	{
	case 'W':
	  game_add(player_add(result_whitename(res), 0, 0, 0),
		   player_add(result_blackname(res), 0, 0, 0),
		   -blacks_advantage(result_komi(res), result_handicap(res)),
		   weight, date);
	  break;
	case 'B':
	  game_add(player_add(result_blackname(res), 0, 0, 0),
		   player_add(result_whitename(res), 0, 0, 0),
		   blacks_advantage(result_komi(res), result_handicap(res)),
		   weight, date);
	  break;
	}
      }
    }
    if (ferror(fin))
      perrex("Error on%s input", (fin == stdin ? " standard" : ""));
    if (fin != stdin)
      fclose(fin);
    result_destroy(res);
  }

  timer_print(fdiag, "\nRead input in");
  if (fdiag)
    fprintf(fdiag,
	    "\nInitial value: %5.2f\n\nInput:\n%6lu players\n%6lu games\n",
	    meanstart,
	    (unsigned long)player_count(), (unsigned long)game_count());

  mlrate(meanstart, fdiag);

  timer_print(fdiag, "Rated in");

  mlrate_errors(fdiag);

  timer_print(fdiag, "\nComputed errors in");

  print_ratings(fout, dbname, fgraph, fdiag);

  timer_print(fdiag, "\nWrote ratings in");

  exit(0);
}

/*
**  Print the rated players on stdout, and some statisticts.
*/
static void
print_ratings(FILE *fout, char *dbname, FILE *fgraph, FILE *fdiag)
{
  struct { char rs[4]; size_t count; } stats[(unsigned)IRANK_DAN_ICEILING+1];
  unsigned i;
  player_t p;
  piter_t piter;
  unsigned pcount = 0;
  irank_t rsum = 0.0;
  double mean;
  rdbm_t db = NULL;

  i = (unsigned)IRANK_DAN_ICEILING + 1;
  while (i--)
  {
    iranktostr((irank_t)i, stats[i].rs, sizeof(stats[i].rs));
    stats[i].count = 0;
  }

  if (dbname)
    if (!(db = rdbm_create(dbname)))
      errex("rdbm_create(\"%s\") failed\n", dbname);

  player_sortbyrank_desc();
  rdbm_printheader(fout);

  player_start(&piter);
  while ((p = player_next(&piter)) != NO_PLAYER)
    if (player_get_rated(p) && !player_get_ignore(p))
    {
      rdbm_player_t rp;
      unsigned ri;
      double r = (double)player_get_rank(p);
      double low = player_get_low(p);
      double high = player_get_high(p);

      if (high - low > Max_rank_span)
	continue;
      ri = (unsigned)r;
      if (ri > 36)
	ri = 36;
      stats[ri].count += 1;
      rsum += r;
      pcount += 1;

      strncpy(rp.name, player_get_name(p), RDBM_NAMEMAX);
      rp.name[RDBM_NAMEMAX] = '\0';
      strcpy(rp.rank, stats[ri].rs);
      if (high - low <= Max_star_span)
	rp.star = 1;
      else
	rp.star = 0;
      rp.rating = r;
      rp.wins = player_get_wins(p);
      rp.losses = player_get_losses(p);
      rp.wwins = player_get_wwins(p);
      rp.wlosses = player_get_wlosses(p);
      rp.error = 0;
      rp.low = low;
      rp.high = high;
      rp.lastplayed = player_get_lastplayed(p);
      if (db)
	if (!rdbm_store(db, &rp))
	  errex("rdbm_store(..., \"%s, ...) failed\n", rp.name);

      /* Print the player and ratings stuff. */
      rdbm_printline(fout, &rp);
    }
  if (db)
    rdbm_close(db);

  /*
  ** Print the statistics.
  */
  for (i = 0 ; i < 37 && stats[i].count == 0 ; i++)
    ;
  for ( ; i < 37 ; i++)
  {
    int j;
    double perc = (100.0*stats[i].count)/pcount;

    fprintf(fgraph, "%4s: %-5lu|",
	    (stats[i].rs[0] ? stats[i].rs : ""),
	    (unsigned long)stats[i].count);

    j = (perc == 0.0 ? 0 : (int)(perc+1.0));
    while (j-- > 0)
      fputc('*', fgraph);
    j = 30 - (perc == 0.0 ? 0 : (int)(perc+1.0));
    while (j-- > 0)
      fputc(' ', fgraph);
    fprintf(fgraph, "| (%.2f%%)\n", perc);
  }
  rsum /= pcount;
  mean = player_get_sorted_mean();
  fprintf(fgraph,
	  "\n%5u rated players. Average rating: %5.2f (%s), mean %5.2f (%s)\n",
	  pcount,
	  rsum, stats[(unsigned)rsum].rs,
	  mean, stats[(unsigned)mean].rs);
}


#if 0
static void
read_ratings(FILE *f, FILE *fdiag)
{
  size_t line = 0;
  int errflag = 0;
  char buf[LINEBUFSIZE];

  while (fgets(buf, LINEBUFSIZE, f) != NULL)
  {
    irank_t rank;
    char *name, *s;

    line += 1;
    s = buf + strspn(buf, "\t\n\r ");
    if (!s[0] || s[0] == '#')
      continue;
    s = strtok(s, "\"");
    if (!s)
    {
      error("Missing \" at line %d in ratings file\n", line);
      errflag = 1;
      continue;
    }
    name = s;
    s = strtok(NULL, "\t\n\r ");
    if (!strtoirank(s, &rank))
    {
      error("Bad rank in ratings file at line %d: %s\n", line, s);
      errflag = 1;
      continue;
    }
    player_add(name, rank, 1, 0);
  }
  if (errflag)
    return;
  if (ferror(f))
    perrex("while reading ratings file\n");
}
#endif


static void
read_ignore(FILE *f, FILE *fdiag)
{
  char buf[LINEBUFSIZE];

  while (fgets(buf, LINEBUFSIZE, f) != NULL)
  {
    char *s = buf + strspn(buf, "\t ");

    if (*s != '#')
    {
      char *e = s + strlen(s);

      if (s == e)
	continue;
      /* Remove trailing blanks. */
      do {
	e -= 1;
      } while (s <= e && (*e == ' ' || *e == '\t' || *e == '\n'));
      *++e = '\0';
      if (s < e)
      {
	player_add(s, 0.0, 0, 1);	/* Ignore me. */
	if (fdiag)
	  fprintf(fdiag, "Ignoring %s\n", s);
      }
    }
  }
}


static time_t Wall_t0;
static clock_t CPU_t0;

static void
timer_start(void)
{
  Wall_t0 = time(NULL);
  CPU_t0 = clock();
}

static void
timer_print(FILE *fdiag, char *s)
{
  time_t wall_t1 = time(NULL);
  clock_t cpu_t1 = clock();

  if (fdiag)
    fprintf(fdiag, "%s %lu seconds (%g s CPU time)\n",
	    s, wall_t1 - Wall_t0,
	    ((double)(cpu_t1 - CPU_t0))/CLOCKS_PER_SEC);
  timer_start();
}
