/* $Id: list.c,v 1.2 2002/01/27 16:02:55 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1993-01-17, 1997.
**
** A general simple list.
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
#include "list.h"

static list_t list_allocate(void);

/* Constructor. */

#define CONS_FUN(Name, Type, Tag, Element) \
list_t \
  Name (Type car, list_t cdr) \
{ \
  list_t c = list_allocate(); \
\
  if (c) \
    { \
      c->element.type = Tag; \
      c->element.value.Element = car; \
      list_cons(c, cdr); \
    } \
  return c; \
}

CONS_FUN(list_cons_pointer, l_pointer_t, lt_pointer, e_pointer)
CONS_FUN(list_cons_integer, l_integer_t, lt_integer, e_integer)
CONS_FUN(list_cons_uinteger, l_uinteger_t, lt_uinteger, e_uinteger)
CONS_FUN(list_cons_float, l_float_t, lt_float, e_float)


/* Accessors. */

list_t
list_last(list_t l)
{
  if (l)
    while (l->next)
      l = l->next;
  return l;
}

size_t
list_length(list_t l)
{
  size_t len = 0;

  while (l)
  {
    len += 1;
    l = l->next;
  }
  return len;
}

#define FIND_FUN(Name, Type, Tag, Element) \
list_t \
Name (Type elem, list_t l, int (*fp)()) \
{ \
  while (l) \
  { \
    if (l->element.type == Tag && fp (elem, l->element.value.Element)) \
      return l; \
    else \
      l = l->next; \
  } \
  return NULL; \
}

FIND_FUN(list_find_pointer, l_pointer_t, lt_pointer, e_pointer)
FIND_FUN(list_find_integer, l_integer_t, lt_integer, e_integer)
FIND_FUN(list_find_uinteger, l_uinteger_t, lt_uinteger, e_uinteger)
FIND_FUN(list_find_float, l_float_t, lt_float, e_float)


/* Destructors. */

void
list_delete_cell(list_t c, list_t *lp)
{
  list_t l = *lp;

  if (c == l)
  {
    *lp = l->next;
    free(c);
    return;
  }
  while (l->next)
    if (c != l->next)
      l = l->next;
    else
    {
      l->next = l->next->next;
      free(c);
      return;
    }
}

#define DELETE_FUN(Name, Type, Tag, Element) \
int \
Name (Type elem, list_t *lp, int (*fp)()) \
{ \
  list_t l = *lp; \
\
  if (l->element.type == Tag && fp (elem, l->element.value.Element)) \
  { \
    *lp = l->next; \
    free(l); \
    return 1; \
  } \
  while (l->next) \
    if (l->next->element.type != Tag || \
	! fp (elem, l->next->element.value.Element)) \
      l = l->next; \
    else \
    { \
      l->next = l->next->next; \
      free(l->next); \
      return 1; \
    } \
  return 0; \
}

DELETE_FUN(list_delete_pointer,l_pointer_t,lt_pointer,e_pointer)
DELETE_FUN(list_delete_integer,l_integer_t,lt_integer,e_integer)
DELETE_FUN(list_delete_uinteger,l_uinteger_t,lt_uinteger,e_uinteger)
DELETE_FUN(list_delete_float,l_float_t,lt_float,e_float)

void
list_delete(list_t l)
{
  while (l)
  {
    list_t c = l;

    l = l->next;
    free(c);
  }
}


static list_t
list_allocate(void)
{
  list_t l = (list_t)malloc(sizeof(struct list));

  if (l)
    memset(l, 0, sizeof(struct list));
  return l;
}

