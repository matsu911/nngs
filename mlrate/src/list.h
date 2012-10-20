/* $Id: list.h,v 1.2 2002/01/27 16:02:55 pem Exp $
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

#ifndef _list_h_
#define _list_h_

#include <stddef.h>
#include "list_types.h"

/* Use the functions decleare below. However, if you want to do
** destruction on the structure, feel free to do so, at your own
** risk of course.
*/
typedef struct list
{
  list_element_t element;
  struct list *next;
} *list_t;

/* Constructor. */

#define list_cons(C, L)  ((C)->next = (L))

/* Returns NULL if malloc() fails. */
extern list_t
  list_cons_pointer(l_pointer_t car, list_t cdr);
extern list_t
  list_cons_integer(l_integer_t car, list_t cdr);
extern list_t
  list_cons_uinteger(l_uinteger_t car, list_t cdr);
extern list_t
  list_cons_float(l_float_t car, list_t cdr);


/* Accessors. */

#define list_first_type(L)     ((L)->element.type)
#define list_first_pointer(L)  ((L)->element.value.e_pointer)
#define list_first_integer(L)  ((L)->element.value.e_integer)
#define list_first_uinteger(L) ((L)->element.value.e_uinteger)
#define list_first_float(L)    ((L)->element.value.e_float)

#define list_rest(L)  ((L) ? (L)->next : NULL)

extern list_t
  list_last(list_t);

extern size_t
  list_length(list_t);


/* The comparison function should take two void* as argument and return
** non-zero when equal, and 0 when not equal.
*/
extern list_t
  list_find_pointer(l_pointer_t elem, list_t, int (*)());
extern list_t
  list_find_integer(l_integer_t elem, list_t, int (*)());
extern list_t
  list_find_uinteger(l_uinteger_t elem, list_t, int (*)());
extern list_t
  list_find_float(l_float_t elem, list_t, int (*)());


/* Destructors. */

/* Returns 1 if the element was found and deleted, 0 otherwise, and
** updates the list pointer if needed.
** The cons cell pointing to the element is destructed, but not
** the element pointed at.
*/
extern void
  list_delete_cell(list_t, list_t *);

extern int
  list_delete_pointer(l_pointer_t elem, list_t *, int (*)());
extern int
  list_delete_integer(l_integer_t elem, list_t *, int (*)());
extern int
  list_delete_uinteger(l_uinteger_t elem, list_t *, int (*)());
extern int
  list_delete_float(l_float_t elem, list_t *, int (*)());

/* Destruct the whole list. If the cell elements are pointing to other
** data this is not freed.
** This most useful when integers of floats are stored in the list.
*/
extern void
  list_delete(list_t);

#endif /* _list_h_ */
