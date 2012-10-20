/* $Id: cmd.h,v 1.2 2002/01/27 16:02:54 pem Exp $
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

#ifndef _cmd_h
#define _cmd_h

typedef enum
{
  cmd_none,
  cmd_string,
  cmd_integer,
  cmd_unsigned,
  cmd_double
} cmd_arg_t;

typedef enum
{
  cmd_err_none = 0,
  cmd_err_cmd,			/* Unknown command */
  cmd_err_narg,			/* Missing argument */
  cmd_err_type,			/* Wrong type of argument */
  cmd_err_xarg,			/* Argument when none is expected */
  cmd_err_tab,			/* Something wrong in the cmd table */
  cmd_err_mem			/* Failed to malloc() */
} cmd_err_t;

typedef struct
{
  char *cmd;
  cmd_err_t err;
  cmd_arg_t type;
  union
  {
    char *s;			/* malloc'ed string. */
    int i;
    unsigned u;
    double d;
  } arg;
} cmd_t;


typedef struct
{
  char *cmd;
  int code;
  cmd_arg_t type;
} cmd_tab_t;


/* Reads a command line from and puts the argument in 'a'.
** The table should be terminated by a cmd_tab_t with a NULL pointer
** 'cmd'.
** 'linep' should point at a line counter which is initially set to 0;
** it's updated after each call to the current line number.
** Returns t->code on success, -1 on error (if a->err == cmd_err_none it's
** just EOF).
*/
extern int cmd_read(cmd_tab_t *t, cmd_t *a, FILE *fp, unsigned *linep);

#endif /* _cmd_h */
