/* $Id: shash.h,v 1.2 2002/01/27 16:02:57 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1993-06-21, 1996, 1997.
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

#ifndef _shash_h_
#define _shash_h_

typedef struct shash *shash_t;

/* Don't even *think* you can poke into this structure! */
typedef struct
{
  void *l; unsigned i;
} shash_iter_t;

/* Create a hash table for at least 'size' elements (it may be bigger). */
extern shash_t
  shash_create(unsigned size);

/* Destroy the hash table. */
extern void
  shash_destroy(shash_t table);

/* Remove all elements in the table. */
extern void
  shash_clear(shash_t table);

/* Return the current size of the table. */
extern unsigned
  shash_size(shash_t table);

/* Return the number of elements in the table. */
extern unsigned
  shash_count(shash_t table);

/* Return the max percentage limit for the table.
** If the table is filled to this limit it's rehashed to
** the double size.
*/
extern unsigned
  shash_max(shash_t table);

/* Return the min percentage limit for the table.
** If the table is filled below this limit it's shrunk to
** the half size.
*/
extern unsigned
  shash_min(shash_t table);

/* Set the max percentage limit. */
extern void
  shash_set_max(shash_t table, unsigned max);

/* Set the min percentage limit. */
extern void
  shash_set_min(shash_t table, unsigned min);

/* The iterator. Example:
**
** {
**   shash_iter_t it;
**   char *key, void *val;
**
**   shash_start(tab, &it);
**   while (shash_next(tab, &it, &key, &val))
**     do_something(key, val);
** }
**
*/
extern void
  shash_start(shash_t table, shash_iter_t *it);
extern int
  shash_next(shash_t table, shash_iter_t *it, char **kp, void **vp);


/* Put the key-value pair into the table.
** Returns 1 if successful.
*/
extern int
  shash_put(char *key, void *val, shash_t table);

/* Get the value of the key in the table.
** Returns 1 if found, and 'val' is set to the value.
*/
extern int
  shash_get(char *key, void **val, shash_t table);

/* Remove the key-value pair from the table.
** Returns 1 if it was found and removed, and 'val' is set to the value.
*/
extern int
  shash_rem(char *key, void **val, shash_t table);

/* Set the pointers to:
**   The number of chains in the table.
**   The length of the shortest chain longer than 0.
**   The average chain length.
**   The length of the longest chain.
*/
extern void
  shash_info(shash_t table,
  	     unsigned *chains,
	     unsigned *chainmin, float *av, unsigned *chainmax);

#endif				/* _shash_h_ */
