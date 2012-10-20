/* variable.h
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995  Erik Van Riper (geek@imageek.york.cuny.edu)
    and John Tromp (tromp@daisy.uwaterloo.ca/tromp@cwi.nl)

    Adapted from:
    fics - An internet chess server.
    Copyright (C) 1993  Richard V. Nash

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifndef VARIABLE_H
#define VARIABLE_H

struct var_list {
  const char *name;
  int (*var_func)(int p, const char *n, const char *v);
} ;

#define VAR_OK 0
#define VAR_NOSUCH 1
#define VAR_BADVAL 2
#define VAR_AMBIGUOUS 3

int var_set(int, const char *, const char *, int *);

extern struct var_list variables[];

#endif /* VARIABLE_H */
