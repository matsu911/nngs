/* $Id: cmd.c,v 1.3 2002/06/24 14:47:13 pem Exp $
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
#include <string.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include "cmd.h"

#define LINEBUFLEN 256
#define WS " \t\n"

int
cmd_read(cmd_tab_t *t, cmd_t *a, FILE *fp, unsigned *linep)
{
  char line[LINEBUFLEN];

  assert(t != NULL && a != NULL && fp != NULL && linep != NULL);

  a->err = cmd_err_none;

  while (fgets(line, LINEBUFLEN, fp) != NULL)
  {
    int i = 0;
    char *p = strtok(line, WS);

    *linep += 1;
    if (p == NULL || *p == '#')
      continue;
    for (i = 0 ; t[i].cmd != NULL ; i++)
      if (strcasecmp(t[i].cmd, p))
	continue;
      else
      {
	char *xp;

	p = strtok(NULL, WS);
	if (p == NULL || *p == '#')
	{
	  if (t[i].type == cmd_none)
	    return i;
	  else
	  {
	    a->err = cmd_err_narg;
	    return -1;
	  }
	}
	switch (t[i].type)
	{
	case cmd_string:
	  if ((a->arg.s = strdup(p)) == NULL)
	  {
	    a->err = cmd_err_mem;
	    return -1;
	  }
	  break;
	case cmd_integer:
	  a->arg.i = strtol(p, &xp, 10);
	  if (p == xp || *p != '\0')
	  {
	    a->err = cmd_err_type;
	    return -1;
	  }
	  break;
	case cmd_unsigned:
	  a->arg.u = strtoul(p, &xp, 10);
	  if (p == xp || *xp != '\0')
	  {
	    a->err = cmd_err_type;
	    return -1;
	  }
	  break;
	case cmd_double:
	  a->arg.d = strtod(p, &xp);
	  if (p == xp || *xp != '\0')
	  {
	    a->err = cmd_err_type;
	    return -1;
	  }
	  break;
	default:
	  a->err = cmd_err_tab;
	  return -1;
	}
	p = strtok(NULL, WS);
	if (p != NULL && *p != '#')
	{
	  a->err = cmd_err_xarg;
	  return -1;
	}
	return i;
      }
    a->err = cmd_err_cmd;
    return -1;
  }
  return -1;
}
