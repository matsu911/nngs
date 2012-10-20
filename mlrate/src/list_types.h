/* $Id: list_types.h,v 1.3 2002/06/24 14:47:13 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1993-01-17, 1997.
**
** Element types used in general lists.
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

#ifndef _list_types_h_
#define _list_types_h_

typedef void* l_pointer_t;	/* Any pointer. */
typedef long l_integer_t;	/* Any unsigned integer. */
typedef unsigned long l_uinteger_t; /* Any signed integer. */
typedef double l_float_t;	/* Any floating point number. */

/* If you want to use your own tags, use lt_max as an offset to the first
** available non-predefined tag.
*/
typedef enum
{
  lt_pointer, lt_integer, lt_uinteger, lt_float, lt_max
} list_type_t;

typedef struct
{
  list_type_t type;
  union
  {
    l_pointer_t e_pointer;
    l_integer_t e_integer;
    l_uinteger_t e_uinteger;
    l_float_t e_float;
  } value;
} list_element_t;

#endif /* _list_types_h_ */
