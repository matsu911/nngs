/* utils.h
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

#ifndef UTILS_H
#define UTILS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>	/* for FILE */
#include <stdarg.h>	/* for va_list varargs */

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>   /* struct stat */
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>	/* DIR, struct dirent */
#endif

#include "bm.h"
#include "language.h"
#include "formats.h"
#include "files.h"

#define MAX_WORD_SIZE 1024

/* Maximum length of an output line */
#define MAX_LINE_SIZE 1024

/* Maximum size of a filename */
#ifdef FILENAME_MAX
#  define MAX_FILENAME_SIZE FILENAME_MAX
#else
#  define MAX_FILENAME_SIZE 1024
#endif
	/* AvK: macro to get number of elements in array
	** The cast is to suppress "comparing int to unsigned"
	** warnings :-[
	*/
#define COUNTOF(a) (int)(sizeof(a)/sizeof(a)[0]) 

	/* AvK: Usage: printf("%s", IFNULL(cp, "Not Provided")); */
#define IFNULL(a,b) (a) ? (a) : (b)
	/* For arrays: this replaces the test by a[0] */
#define IFNULL0(a,b) ((a)&&(a)[0]) ? (a) : (b)


int iswhitespace(int);
char *getword(char *);
/* Returns a pointer to the first whitespace in the argument */
char *eatword(char *);
/* Returns a pointer to the first non-whitespace char in the argument */
char *eatwhite(char *);
/* Returns the next word in a given string >eatwhite(eatword(foo))< */
char *nextword(char *);
/* Return pointers into a static pool. */
char * statstr_new(size_t len);
char * statstr_dup(const char *str, size_t len);
char * statstr_trim(const char *str, size_t len);


int mail_pst(int, const char *, char *);
int mail_psn(int, const char *, const char *);
int mail_asn(const char *, const char *, const char *);
int mail_ast(const char *, const char *, const char *);
int pcommand(int, const char *, ...);
int pprintf(int, const char *, ...);
int cpprintf(int, int, const char *, ...);
int pprintf2(int, int, const char *);
int Logit(const char *, ...);
int pprintf_prompt(int, const char *, ...);
int cpprintf_prompt(int, int, const char *, ...);
int my_vsnprintf(char *, size_t, const char *, va_list);
int my_snprintf(char *, size_t, const char *, ...);
int psend_raw_file(int p, const char *, const char *);
int psend_file(int p, const char *, const char *);
int pxysend_raw_file(int p, int num, ...);
int pxysend_file(int p, int num, ...);
int pmore_file(int p);
int xpsend_dir(int p, int num);

char *stolower(char *);
char *stoupper(char *);

int safechar(int);
int safestring(const char *);
int safefilename(const char *path);
int invalid_pname(const char *);
int printablestring(const char *);
char *mystrdup(const char *);
char * mycrypt(const char *passwd, const char * salt);

char *secs2hms_mask(int secs, int mask);
char *secs2str_short(int secs);
char *secs2hms_long(int secs);

char *tm2str_ccyy_mm_dd(const struct tm *);
char *time2str_sgf(const time_t *);
char *ResultsDate(char *);
char *time2str_local(const time_t *);
char *time2str_utc(const time_t *);
char *time2str_file(const time_t *);

/* unsigned read_tick(void); */
char *tenth_str(unsigned int, int);
int untenths(unsigned int);
size_t do_copy(char *, const char *, size_t);

int truncate_file(char *, int);

int file_has_pname(const char *, const char*);
const char *file_wplayer(const char *);
const char *file_bplayer(const char *);

int xyfilename(char *dst, int num, ...);
FILE * xfopen(const char *name,const char *mode);
FILE * xyfopen(int num, const char *, ...);
FILE * pxyfopen(int p, int num, const char *, ...);
int xyrename(int num1, int num2, ...);
int xylines_file(int num, ...);
int xylink(int num, ...);
int xyunlink(int num, ...);
int xystat(struct stat * sb, int num, ...);
int xytouch(int num, ...);
DIR * xyopendir(int num, ...);
char * filename(void);
int pcn_out(int, int, int, ...);
int pcn_out_prompt(int p, int code, int num, ...);
int fake_vsnprintf(char *dst, size_t dstlen, const char *format, va_list ap);

char *dotQuad(unsigned int);
int asc2ipaddr(char *str, unsigned *add);

int available_space(void);
int file_exists(char *);

int search_directory(char *, int, char *, int, ...);
int display_directory(int, const char *, int);
const char *SendCode(int, int);
char *KillTrailWhiteSpace(char *str);
char *strlwr(char *psz);
void bldsearchdata(char *psz);

int parse_rank(int num, int ch);

#ifndef TICSPERSEC
#define TICSPERSEC (10)
#define SECS2TICS(s) ((s)*TICSPERSEC)
#define TICS2SECS(t) ((t)/TICSPERSEC)
#endif

struct ticker {
  time_t time;
  unsigned tick;
  };
extern struct ticker globclock;
extern unsigned refetch_ticker(void);

#define SDATA_USER 1
#define SDATA_HOST 2

struct searchdata {
  int where;
  struct boyermoore bmData;
} ;

struct searchresult {
  char szPlayer[100];
  char szMailAddr[200];
} ;

extern const struct searchresult *search(char *);
extern int is_totally_blank(char *);

#endif /* UTILS_H */
