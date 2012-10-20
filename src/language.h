/* language.h
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

#ifndef LANGUAGE_H
#define LANGUAGE_H

#define LANGUAGE_ENGLISH 0
#define LANGUAGE_CHINESE 1
#define LANGUAGE_GERMAN 2
#define LANGUAGE_POLISH 3
#define LANGUAGE_FRENCH 4

#define LANGUAGE_DEFAULT 0
#define LANGUAGE_COUNT 5

void langauge_init(void);
const char * find_format(int lang, int num);
/*const char * language_prefix(int lang);*/

const char * language_num2prefix(int lang);
int language_prefix2num(const char * prefix);
void help_init(void);
int search_index(char * buff, size_t buffsiz, const char *needle, int num, int lang);

#endif /* LANGUAGE_H */
