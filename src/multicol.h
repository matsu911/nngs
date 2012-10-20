/* multicol.h
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997  Erik Van Riper (geek@midway.com)
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

#ifndef MULTICOL_H
#define MULTICOL_H

struct multicol
{
  int arraySize;
  int num;
  char **strArray;
} ;

struct multicol *multicol_start(int);
int multicol_store(struct multicol *, const char *);
int multicol_store_sorted(struct multicol *, const char *);
int multicol_pprint(struct multicol *, int, int, int);
int multicol_end(struct multicol *);

#endif /* MULTICOL_H */
