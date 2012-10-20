/* $Id: error.c,v 1.2 2002/01/27 16:02:54 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "error.h"

static char *Progname;

void
error_init(char **argv)
{
  if ((Progname = strrchr(argv[0], '/')))
    Progname += 1;
  else
    Progname = argv[0];
}

char *
error_progname(void)
{
  return Progname;
}

void
warning(char *f, ...)
{
  va_list ap;
  va_start(ap, f);
  fprintf(stderr, "%%%s: ", Progname);
  vfprintf(stderr, f, ap);
  va_end(ap);
}

void
error(char *f, ...)
{
  va_list ap;
  va_start(ap, f);
  fprintf(stderr, "?%s: ", Progname);
  vfprintf(stderr, f, ap);
  va_end(ap);
}

void
errex(char *f, ...)
{
  va_list ap;
  va_start(ap, f);
  fprintf(stderr, "?%s: ", Progname);
  vfprintf(stderr, f, ap);
  va_end(ap);
  exit(1);
}

void
perrex(char *f, ...)
{
  va_list ap;
  va_start(ap, f);
  fprintf(stderr, "?%s: %s: ", Progname, strerror(errno));
  vfprintf(stderr, f, ap);
  va_end(ap);
  exit(1);
}
