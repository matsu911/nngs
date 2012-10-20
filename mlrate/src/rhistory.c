/* $Id: rhistory.c,v 1.4 2002/01/27 17:56:52 pem Exp $
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
#include "error.h"

#include "rdbm.h"
#undef _xdbm_h
#undef xdbm_t
#undef xdbm_create
#undef xdbm_open
#undef xdbm_close
#undef xdbm_store
#undef xdbm_fetch
#undef xdbm_delete
#undef xdbm_start
#undef xdbm_next
#include "hdbm.h"

#define USAGE "Usage: %s -i <rdbm>|-g <player>|-d <player> <hdbm>\n"

#define SECSPERDAY (24 * 60 * 60)

int
main(int argc, char **argv)
{
  hdbm_t hdbm = NULL;
  char *rdbmname = NULL, *player = NULL;
  int c, delflag = 0;
  hdbm_player_t hp;
  time_t day;

  error_init(argv);

  day = time(NULL) / SECSPERDAY;

  opterr = 0;
  while ((c = getopt(argc, argv, "d:g:i:h?")) != EOF)
    switch (c)
    {
    case 'd':
      player = optarg;
      delflag = 1;
      break;
    case 'g':
      player = optarg;
      delflag = 0;
      break;
    case 'i':
      rdbmname = optarg;
      break;
    case 'h': case '?':
    default:
      errex(USAGE, error_progname());
    }

  if (argv[optind] == NULL)
    errex("Missing hdbm argument\n");

  if (!player && !rdbmname)
    errex("One of -i, -g or -d required\n");

  if (player && rdbmname)
    errex("Only one of -i and %s is allowed\n",
	  (delflag ? "-d" : "-g"));

  if (rdbmname)
  {
    rdbm_player_t rp;
    rdbm_t rdbm;

    if ((hdbm = hdbm_create(argv[optind])) == NULL)
      errex("hdbm_create(\"%s\") failed\n", argv[optind]);

    if ((rdbm = rdbm_open(rdbmname, 0)) == NULL)
      errex("rdbm_open(\"%s\") failed\n", rdbmname);
    rdbm_start(rdbm);
    while (rdbm_next(rdbm, &rp))
    {
      unsigned i;

      if (!hdbm_fetch(hdbm, rp.name, &hp))
      {
	memset(&hp, 0, sizeof(hp));
	strncpy(hp.name, rp.name, PLAYER_MAX_NAMELEN);
	hp.name[PLAYER_MAX_NAMELEN] = '\0';
	i = 0;
      }
      else
      {
	i = hp.next;
	do {
	  if (hp.point[i].day == day)
	    break;
	} while ((i = (i + 1) % HDBM_MAX_POINTS) != hp.next);
      }
      hp.point[i].rating = (short)((rp.rating * 100) + 0.5);
      hp.point[i].low = (short)((rp.low * 100) + 0.5);
      hp.point[i].high = (short)((rp.high * 100) + 0.5);
      hp.point[i].day = day;
      if (i == hp.next)
	hp.next = (hp.next + 1) % HDBM_MAX_POINTS;
      if (!hdbm_store(hdbm, &hp))
	errex("hdbm_store(\"%s\") failed\n", hp.name);
    }
    rdbm_close(rdbm);
  }
  else if (delflag == 0)
  {
    unsigned i;
    hdbm_point_t lastpoint;

    if ((hdbm = hdbm_open(argv[optind], 0)) == NULL)
      errex("hdbm_open(\"%s\", 0) failed\n", argv[optind]);

    lastpoint.day = 0;
    if (!hdbm_fetch(hdbm, player, &hp))
      errex("Player not found: \"%s\"\n", player);
    printf("%s\n", hp.name);
    i = hp.next;
    do {
      if (hp.point[i].day)
      {
	time_t t;
	struct tm *tp;

	if (lastpoint.day && hp.point[i].day - lastpoint.day > 1)
	  while (lastpoint.day < hp.point[i].day)
	  {
	    t = (time_t)lastpoint.day * SECSPERDAY;
	    tp = localtime(&t);
	    printf("%d-%02d-%02d\t%5hd\t%5hd\t%5hd\n",
		   1900+tp->tm_year, tp->tm_mon+1, tp->tm_mday,
		   lastpoint.rating, lastpoint.low, lastpoint.high);
	    lastpoint.day += 1;
	  }
	else
	{
	  t = (time_t)hp.point[i].day * SECSPERDAY;
	  tp = localtime(&t);
	  printf("%d-%02d-%02d\t%5hd\t%5hd\t%5hd\n",
		 1900+tp->tm_year, tp->tm_mon+1, tp->tm_mday,
		 hp.point[i].rating, hp.point[i].low, hp.point[i].high);
	}
	lastpoint = hp.point[i];
      }
    } while ((i = (i + 1) % HDBM_MAX_POINTS) != hp.next);
  }
  else
  {
    if ((hdbm = hdbm_open(argv[optind], 1)) == NULL)
      errex("hdbm_open(\"%s\", 1) failed\n", argv[optind]);

    if (hdbm_delete(hdbm, player))
      printf("\"%s\" deleted\n", player);
    else
      errex("hdbm_delete(\"%s\") failed\n", player);
  }

  hdbm_close(hdbm);
  exit(0);
}
