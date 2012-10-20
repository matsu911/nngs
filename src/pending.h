
/* playerdb.h
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995  Erik Van Riper (geek@nngs.cosmic.org)
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


#ifndef PENDING_H
#define PENDING_H

#define PEND_MATCH 0
#define PEND_ADJOURN 1
#define PEND_PAUSE 2
#define PEND_GMATCH 3 /* params 1=color 2=size 3=time 4=byo-time */
#define PEND_KOMI 4
#define PEND_DONE 5
#define PEND_LADDER 6
#define PEND_PAIR 7
#define PEND_TEACH 8
#define PEND_AUTOMATCH 9
#define PEND_GOEMATCH 10
#define PEND_TMATCH 11

struct pending {
  struct pending **hnd,*nxt;
  unsigned is_valid: 1;
  unsigned seq;
  int type;
  int whoto;
  int whofrom;
  int param1, param2, param3, param4, param5;
  float float1;
} ;


void pending_init(void);
struct pending * pending_new(int from, int to, int type);
void pending_delete(struct pending * ptr);

int pending_count(int from, int to, int type);
struct pending * pending_find(int from, int to, int type);
struct pending * pending_next(struct pending *ptr, int fr, int to, int type);

#endif /* PENDING_H */
