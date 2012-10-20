/* $Id: old2newres.c,v 1.4 2002/01/27 18:03:01 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1998-03.
**
** Converts the old result file format to the new one.
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
#include "result.h"
#include "error.h"

static void
prt_name(char *name)
{
  putchar('"');
  while (*name)
  {
    switch (*name)
    {
    case '\\':
    case '"':
      putchar('\\');
      break;
    }
    putchar(*name);
    name += 1;
  }
  putchar('"');
}

static void
prt_rank(irank_t r, int ranked)
{
  if (!ranked)
    putchar('-');
  else
  {
    char rank[8];

    iranktostr(r, rank, sizeof(rank));
    fputs(rank, stdout);
  }
}

int
main(int argc, char **argv)
{
  int x;
  result_t res;

  error_init(argv);

  res = result_create(stdin);
  if (res == NULL)
    errex("result_create(stdin) failed");
  while ((x = result_read(res)) > -1)
    if (x == 0)
    {
      time_t t = result_date(res);
      struct tm *tp = localtime(&t);

      prt_name(result_whitename(res));
      putchar(' ');
      prt_rank(result_whiterank(res), result_wrated(res));
      putchar(' ');
      prt_name(result_blackname(res));
      putchar(' ');
      prt_rank(result_blackrank(res), result_brated(res));
      printf(" %u %g %c %d-%02d-%02d\n",
	     result_handicap(res), result_komi(res), result_winner(res),
	     1900+tp->tm_year, tp->tm_mon+1, tp->tm_mday);
    }
  exit(0);
}
