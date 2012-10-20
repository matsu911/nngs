/* $Id: error.h,v 1.2 2002/01/27 16:02:54 pem Exp $
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

#ifndef _error_h
#define _error_h

/* Initialize the error routines (basically, extracts the program name. */
extern void error_init(char **argv);

/* Returns the program name. */
extern char *error_progname(void);

/*
** All error routines takes a "printf" format string
** and optional arguments following that.
*/

/* Prints a warning on stderr. */
extern void warning(char *f, ...);

/* Prints an error message on stderr. */
extern void error(char *f, ...);

/* Prints and error message on stderr, and then exits. */
extern void errex(char *f, ...);

/* Prints an error message, followed by an error message
** associated with the current 'errno', and then exits.
*/
extern void perrex(char *f, ...);

#endif /* _error_h */
