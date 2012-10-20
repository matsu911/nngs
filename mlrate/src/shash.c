/* $Id: shash.c,v 1.2 2002/01/27 16:02:57 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1993-06-21, 1996-05-07, 1997.
**
** 1996-05-07: Changed the shash_make() to shash_create(), and
** rewrote the iterator stuff (and consequently, the grow and
** shrink functions).
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
#include <string.h>
#include <limits.h>
#include "shash.h"
#include "list.h"
#include "ilog2.h"

#define DEFAULT_MAX 88
#define DEFAULT_MIN 16

#define DEBUG 1
#ifdef DEBUG
#include <stdio.h>
#endif

typedef unsigned char uchar;

struct shash
{
  unsigned size, minsize, count, max, min;
  list_t *table;
};


static unsigned T[] =
{
  1, 87, 49, 12, 176, 178, 102, 166, 121, 193, 6, 84, 249, 230, 44, 163,
  14, 197, 213, 181, 161, 85, 218, 80, 64, 239, 24, 226, 236, 142, 38, 200,
  110, 177, 104, 103, 141, 253, 255, 50, 77, 101, 81, 18, 45, 96, 31, 222,
  25, 107, 190, 70, 86, 237, 240, 34, 72, 242, 20, 214, 244, 227, 149, 235,
  97, 234, 57, 22, 60, 250, 82, 175, 208, 5, 127, 199, 111, 62, 135, 248,
  174, 169, 211, 58, 66, 154, 106, 195, 245, 171, 17, 187, 182, 179, 0, 243,
  132, 56, 148, 75, 128, 133, 158, 100, 130, 126, 91, 13, 153, 246, 216, 219,
  119, 68, 223, 78, 83, 88, 201, 99, 122, 11, 92, 32, 136, 114, 52, 10,
  138, 30, 48, 183, 156, 35, 61, 26, 143, 74, 251, 94, 129, 162, 63, 152,
  170, 7, 115, 167, 241, 206, 3, 150, 55, 59, 151, 220, 90, 53, 23, 131,
  125, 173, 15, 238, 79, 95, 89, 16, 105, 137, 225, 224, 217, 160, 37, 123,
  118, 73, 2, 157, 46, 116, 9, 145, 134, 228, 207, 212, 202, 215, 69, 229,
  27, 188, 67, 124, 168, 252, 42, 4, 29, 108, 21, 247, 19, 205, 39, 203,
  233, 40, 186, 147, 198, 192, 155, 33, 164, 191, 98, 204, 165, 180, 117, 76,
  140, 36, 210, 172, 41, 54, 159, 8, 185, 232, 113, 196, 231, 47, 146, 120,
  51, 65, 28, 144, 254, 221, 93, 189, 194, 139, 112, 43, 71, 109, 184, 209
};

#define MIN_SIZE 8
#define MED_SIZE 0x100
#define MAX_SIZE 0x10000


/*
**  The entry structure.
*/

typedef struct
{
  char point;			/* 1 if key's a pointer, 0 otherwise. */
  union {
	  uchar key[8];		/* Don't malloc short keys. */
	  uchar *keyp;
	} key;
  void *value;
} shash_ent_t;

#define ENT_KEYP(E) ((E)->point ? (E)->key.keyp : (E)->key.key)

static int
compent(uchar *s1, shash_ent_t *e)
{
  return (!strcmp((char *)s1, (char *)ENT_KEYP(e)));
}

static shash_ent_t *
entry_create(uchar *key, size_t len)
{
  shash_ent_t *e;

  if ((e = (shash_ent_t *)malloc(sizeof(shash_ent_t))))
  {
    e->value = 0;
    if (len >= 8)
    {
      e->point = 1;
      if ((e->key.keyp = (uchar *)strdup((char *)key)) == NULL)
	free(e);
    }
    else
    {
      e->point = 0;
      strcpy((char *)e->key.key, (char *)key);
    }
  }
  return e;
}

static void
entry_destroy(shash_ent_t *e)
{
  if (e)
  {
    if (e->point && e->key.keyp)
      free(e->key.keyp);
    free(e);
  }
}


/*
** The hash functions.
*/

static unsigned
H(uchar *str, size_t *len)
{
  register size_t l = 0;
  register unsigned h = 0;

  while (*str)
  {
    h = T[h ^ *str++];
    l += 1;
  }
  if (len)
    *len = l;
  return h;
}

static unsigned
H2(uchar first, uchar *rest)
{
  register unsigned h = T[(unsigned)first];

  while (*rest)
    h = T[h ^ *rest++];
  return h;
}

static unsigned
H16(uchar *str, size_t *len)
{
  unsigned h = H(str, len);

  h = ((h << CHAR_BIT) | H2((str[0]+1) % 256, str+1));
  return h;
}


/*
**  List functions.
*/

static shash_ent_t *
shlist_add(shash_t table, unsigned i, uchar *key, size_t len)
{
  list_t cell;
  shash_ent_t *ent;

  if ((cell = list_find_pointer(key, table->table[i], compent)))
    return list_first_pointer(cell);
  if (!(ent = entry_create(key, len)))
    return NULL;
  if ((cell = list_cons_pointer(ent, table->table[i])))
    table->table[i] = cell;
  else
  {
    entry_destroy(ent);
    return NULL;
  }
  table->count += 1;
  return ent;
}

static shash_ent_t *
shlist_get(shash_t table, unsigned i, uchar *key)
{
  list_t cell;

  if ((cell = list_find_pointer(key, table->table[i], compent)))
    return list_first_pointer(cell);
  else
    return NULL;
}

static int
shlist_rem(shash_t table, unsigned i, uchar *key, void *val)
{
  list_t cell;
  uchar **vp = val;

  if ((cell = list_find_pointer(key, table->table[i], compent)))
  {
    shash_ent_t *ent = list_first_pointer(cell);

    if (vp)
      *vp = ent->value;
    list_delete_cell(cell, &table->table[i]);
    entry_destroy(ent);
    table->count -= 1;
    return 1;
  }
  return 0;
}

static void
shlist_clear(list_t l)
{
  while (l)
  {
    entry_destroy((shash_ent_t *)list_first_pointer(l));
    list_delete_cell(l, &l);
  }
}

static void
shlist_destroy(shash_t table)
{
  if (table->table)
  {
    unsigned i;

    for (i = 0 ; i < table->size ; i += 1)
      shlist_clear(table->table[i]);
    free(table->table);
    table->table = NULL;
  }
}


/*
**  External functions.
*/

shash_t
shash_create(unsigned size)
{
  shash_t tab;

  if ((tab = (shash_t)malloc(sizeof(struct shash))) == NULL)
    return NULL;
  if (size < MIN_SIZE)
    size = MIN_SIZE;		/* Don't make it smaller. */
  else if (size > MAX_SIZE)
  {				/* Fail if we can't make it big enough. */
    free(tab->table);
    free(tab);
    return NULL;
  }
  else				/* Make sure it's a power of 2. */
    size = 1 << (ilog2(size-1)+1);
  if ((tab->table = (list_t *)calloc((size_t)size, sizeof(list_t))) == NULL)
  {
    free(tab);
    return NULL;
  }
  tab->size = size;
  tab->minsize = size;
  tab->count = 0;
  tab->max = DEFAULT_MAX;
  tab->min = DEFAULT_MIN;
  return tab;
}

void
shash_destroy(shash_t table)
{
  if (table)
  {
    shlist_destroy(table);
    free(table);
  }
}

void
shash_clear(shash_t table)
{
  unsigned i;

  for (i = 0 ; i < table->size ; i += 1)
  {
    shlist_clear(table->table[i]);
    table->table[i] = NULL;
  }
  table->count = 0;
  if (table->size > table->minsize)
  {
    list_t *p = (list_t *)realloc(table->table,
				  table->minsize * sizeof(list_t));
    if (p != table->table)
    {
      memset(p, 0, table->minsize * sizeof(list_t));
      table->table = p;
    }
    table->size = table->minsize;
    for (i = 0 ; i < table->size ; i += 1)
      table->table[i] = NULL;
  }
}

unsigned
shash_size(shash_t table)
{
  return table->size;
}

unsigned
shash_count(shash_t table)
{
  return table->count;
}

unsigned
shash_max(shash_t table)
{
  return table->max;
}

unsigned
shash_min(shash_t table)
{
  return table->min;
}

void
shash_set_max(shash_t table, unsigned max)
{
  if (0 < max && max <= 100)
    table->max = max;
}

void
shash_set_min(shash_t table, unsigned min)
{
  if (0 <= min && min < 100)
    table->min = min;
}

void
shash_start(shash_t table, shash_iter_t *it)
{
  it->l = NULL;
  it->i = 0;
}

int
shash_next(shash_t table, shash_iter_t *it, char **kp, void **vp)
{
  if (!it->l)
  {
    unsigned i;

    for (i = it->i ; i < table->size ; i++)
      if (table->table[i])
      {
	it->l = table->table[i++];
	break;
      }
    it->i = i;			/* The next slot. */
  }
  if (it->l)
  {
    shash_ent_t *ent = (shash_ent_t *)list_first_pointer((list_t)it->l);

    if (kp)
      *kp = (char *)ENT_KEYP(ent);
    if (vp)
      *vp = ent->value;
    it->l = list_rest((list_t)it->l);
    return 1;
  }
  return 0;
}    

static int
maybe_grow_table(shash_t table)
{
  if ((100L * table->count) / table->size >= table->max)
  {
    shash_t tmptab;

    /* Grow table and rehash. */
    if ((tmptab = shash_create(table->size * 2)) == NULL)
      return 0;
    else
    {
      shash_iter_t it;
      char *key;
      void *val;

      tmptab->max = table->max;
      tmptab->min = table->min;
      shash_start(table, &it);
      while (shash_next(table, &it, &key, &val))
	if (!shash_put(key, val, tmptab))
	{
	  shash_destroy(tmptab);
	  return 0;
	}
      /* Get rid of the old table->table */
      shlist_destroy(table);
      /* Copy everything from tmptable to table. */
#ifdef DEBUG
      if (table->count != tmptab->count)
	fprintf(stderr, "Data lost! Old count: %d, new: %d\n",
		table->count, tmptab->count);
#endif
      table->count = tmptab->count; /* Not necessary. */
      table->size = tmptab->size;
      table->table = tmptab->table;
      tmptab->table = NULL;
      /* Now get rid of tmptable. */
      shash_destroy(tmptab);
    }
  }
  return 1;
}

int
shash_put(char *key, void *val, shash_t table)
{
  shash_ent_t *ent;
  unsigned h;
  size_t len;

  if (!maybe_grow_table(table))
    return 0;
  h = (table->size < MED_SIZE ? H((uchar *)key, &len) :
       H16((uchar *)key, &len));
  if ((ent = shlist_add(table, h % table->size, (uchar *)key, len)))
  {
    ent->value = val;
    return 1;
  }
  return 0;
}

int
shash_get(char *key, void **val, shash_t table)
{
  shash_ent_t *ent;
  unsigned h;

  h = (table->size < MED_SIZE ? H((uchar *)key, NULL) :
       H16((uchar *)key, NULL));
  if ((ent = shlist_get(table, h % table->size, (uchar *)key)))
  {
    if (val)
      *val = ent->value;
    return 1;
  }
  return 0;
}

int
shash_rem(char *key, void **val, shash_t table)
{
  unsigned h;

  if (((100L * table->count) / table->size < table->min) &&
      table->size / 2 >= table->minsize)
  {
    shash_t tmptab;

    /* Shrink table and rehash. */
    if ((tmptab = shash_create(table->size / 2)))
      return 0;
    else
    {
      shash_iter_t it;
      char *key2;
      void *val;

      tmptab->max = table->max;
      tmptab->min = table->min;
      shash_start(table, &it);
      while (shash_next(table, &it, &key2, &val))
	if (!shash_put(key2, val, tmptab))
	{
	  shash_destroy(tmptab);
	  return 0;
	}
      /* Get rid of the old table->table. */
      shlist_destroy(table);
      /* Copy everything from tmptab to table. */
      table->size = tmptab->size;
      table->count = tmptab->count;
      table->table = tmptab->table;
      tmptab->table = NULL;
      /* Drop the tmptable. */
      shash_destroy(tmptab);
    }
  }

  h = (table->size < MED_SIZE ? H((uchar *)key, NULL) :
       H16((uchar *)key, NULL));
  return shlist_rem(table, h % table->size, (uchar *)key, val);
}

void
shash_info(shash_t table,
	   unsigned *chains, unsigned *chainmin, float *av, unsigned *chainmax)
{
  unsigned i;
  unsigned count = 0, max = 0, min = table->size;
  float sum = 0.0;

  for (i = 0 ; i < table->size ; i += 1)
    if (table->table[i])
    {
      unsigned len = list_length(table->table[i]);

      count += 1;
      sum += (float)len;
      if (len > max)  max = len;
      if (len < min)  min = len;
    }
  if (chains)
    *chains = count;
  if (chainmin)
    *chainmin = min;
  if (av)
    *av = sum/count;
  if (chainmax)
    *chainmax = max;
}
