/* $Id: rdbmtool.c,v 1.3 2002/01/27 17:56:52 pem Exp $
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
#include <string.h>
#include <unistd.h>
#include "rdbm.h"
#include "error.h"

#define USAGE "%s -d <dbname> [-g name] [-r name] [-l]\n"

int
main(int argc, char **argv)
{
  char *dbname = NULL, *gname = NULL, *dname = NULL;
  int list = 0;
  int c;
  rdbm_t db;
  rdbm_player_t rp;

  error_init(argv);

  opterr = 0;
  while ((c = getopt(argc, argv, "d:g:r:lh?")) != EOF)
    switch (c)
    {
    case 'd':
      dbname = optarg;
      break;
    case 'g':
      gname = optarg;
      break;
    case 'r':
      dname = optarg;
      break;
    case 'l':
      list = 1;
      break;
    default:
      errex(USAGE, error_progname());
    }

  if (!dbname)
    errex("-d <dbname> needed\n");
  if (!list && !gname && !dname)
    errex("one of -g <name>, -r <name> or -l needed\n");

  /* Open */
  {
    int w = (dname ? 1 : 0);

    if ((db = rdbm_open(dbname, w)) == NULL)
      errex("Failed to open database: \"%s\" for %s\n",
	    dbname, (w ? "writing" : "reading"));
  }

  /* Lookup */
  if (gname)
  {
    if (!rdbm_fetch(db, gname, &rp))
      printf("Not found in %s: %s\n", dbname, gname);
    else
    {
      rdbm_printheader(stdout);
      rdbm_printline(stdout, &rp);
    }
  }

  /* Delete */
  if (dname)
  {
    if (!rdbm_fetch(db, dname, &rp))
      printf("Not found in %s: %s\n", dbname, dname);
    else
    {
      rdbm_printheader(stdout);
      rdbm_printline(stdout, &rp);
      if (rdbm_delete(db, dname))
	printf("  Deleted\n");
      else
	printf("  Deletion failed\n");
    }
  }

  /* List */
  if (list)
  {
    size_t count = 0;

    rdbm_printheader(stdout);
    rdbm_start(db);
/*     fprintf(stdout, "rdbm_start(db);\n"); */
    while (rdbm_next(db, &rp))
    {
/*       fprintf(stdout, "rdbm_next(db, &rp);\n"); */
      count += 1;
      rdbm_printline(stdout, &rp);
    }
    printf("%5u players\n", count);
  }

  /* Close */
  rdbm_close(db);

  exit(0);
}
