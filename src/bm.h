/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997  J. Alan Eldridge (alane@wozzle.york.cuny.edu)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifndef BOYERMOORE_H
#define BOYERMOORE_H

struct boyermoore {
	int p;
	int delta1[256];
	char *pat;
} ;

int bmInit(char *pat, struct boyermoore *pBM);
char *bmSrch(char *str, struct boyermoore *pBM);

#endif /* BOYERMOORE_H */
