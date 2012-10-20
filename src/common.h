/*
 * common.h
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

#ifndef COMMON_H
#define COMMON_H

#ifndef NULL
#define NULL ((void *)0)
#endif

/*
#define SWAP(a,b,type) {\
  type tmp; \
  tmp = (a);\
  (a) = (b);\
  (b) = tmp;\
}
*/

/*
#ifdef DEBUG
#define ASSERT(expression) \
  while (((expression) ? 0  : \
   (fprintf (stderr, "Assertion failed: file %s, line %d.\n", \
      __FILE__, __LINE__), \
    abort (), 0)))
#else
#define ASSERT(expression) while(0)
#endif
*/

extern unsigned long bytes_received;
extern unsigned long bytes_sent;

#endif /* COMMON_H */
