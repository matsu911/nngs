/* language.c
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996 Erik Van Riper (geek@nngs.cosmic.org)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

	/* NOTE:
	** header file "formats.h" contains *code* for language_init() 
	** which is only active in if WANT_TO_DEFINE(this file) is defined
        ** it should be included *before* other files that include it
	*/
#define WANT_TO_DEFINE 1
#include "formats.h"
#undef WANT_TO_DEFINE
#include "utils.h"
#include "language.h"

struct language {
  int lang;	/* Language number as used in language variable */
  const char *name;
  const char *prefix; /* two letter abbreviation to be used in directorynames, etc */
  const char **formats;
  } languages[] =
{{LANGUAGE_ENGLISH, "english", "en", formats_english}
,{LANGUAGE_CHINESE, "chinese", "ch", formats_chinese}
,{LANGUAGE_GERMAN, "german", "de", formats_german}
,{LANGUAGE_POLISH, "polish", "pl", NULL}
,{LANGUAGE_FRENCH, "french", "fr", NULL}
};

static char badmessage[40];
static void help_add(char *name, int lang);
static void help_init1(int num);

const char * find_format(int lang, int num)
{
const char *format=NULL;
size_t idx;

if (!formats_english[0]) language_init();

/* use english if an invalid language was selected */
  if (lang < 0 || lang >= (int) COUNTOF(languages)) lang = LANGUAGE_DEFAULT;
  
  for(idx=0; idx < COUNTOF(languages); idx++) {
    if (languages[idx].lang != lang) continue;
    format=languages[idx].formats?languages[idx].formats[num]: NULL;
      break;
    }

  /* if no format was found for the particular language, then use the
   * default language (should be english since it's the original server
   * language) */
  if (!format)
    format= num < COUNTOF(formats_default) ?formats_default[num]: NULL;

  /* if it's still not found, that message was never defined */
  if (!format) sprintf(format=&badmessage[0],"Lang_%d,Format_%d", lang,num);

  return format;
}
/* given the languagenumber, return
** a (two letter) prefix to be used in pathnames.
** Fixme: these should be some iso/ansi 2LA abbreviations ?.
*/
const char * language_num2prefix(int lang)
{
  size_t idx;
  const char *prefix="en";

  for(idx=0; idx < COUNTOF(languages); idx++) {
    if (languages[idx].lang != lang) continue;
    prefix=languages[idx].prefix;
    break;
    }

  return (const char *) prefix;
}
/* reversed language lookup.
*/
int language_prefix2num(const char * prefix)
{
  int idx;
  int lang=LANGUAGE_DEFAULT;

  for(idx=0; idx < COUNTOF(languages); idx++) {
    if (strcmp(languages[idx].prefix,prefix)) continue;
    lang=languages[idx].lang;
    break;
  }

  return lang;
}

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

/* struct for administering helpfile availability in all languages */

struct help_avail {
	char * name;	/* commandname/helpfilename */
	unsigned mask;	/* bit masked sigma(1<<lang) */
	};

static struct help_avail * help_ptr=NULL;
static size_t help_used=0;
static size_t help_size=0;

static void help_add(char *name, int lang);
static int help_cmp(const void *l, const void *r);

static int help_cmp(const void *l, const void *r)
{
  const struct help_avail * lh= (const struct help_avail *) l;
  const struct help_avail * rh= (const struct help_avail *) r;

  return strcmp(lh->name,rh->name);
}

static void help_add(char *name, int lang)
{
  size_t idx;

  for(idx=0; idx <help_used; idx++) {
    if (strcmp(help_ptr[idx].name ,name)) continue;
    help_ptr[idx].mask |= (1 << lang);
    return;
    }

  if (help_used == help_size) {
    help_ptr = (help_size) 
      ? realloc(help_ptr , (help_size *=2) * sizeof *help_ptr)
      : malloc((help_size=100) * sizeof *help_ptr );
    }
  help_ptr[help_used].name = mystrdup(name);
  help_ptr[help_used].mask = 1 << lang;
  help_used++;
}

void help_init(void)
{
help_init1(FILENAME_HELP_l);
help_init1(FILENAME_AHELP_l);
}

static void help_init1(int num)
{
  int lang;
  size_t idx;
  DIR * dir;
  FILE *fp;
  struct dirent *dp;
  int cnt;

  for(lang = 0; lang < LANGUAGE_COUNT; lang++) {
    dir = xyopendir(num, lang);
    if (!dir) continue;
    while((dp=readdir(dir))) {
      if (dp->d_name[0] == '.') continue;
      help_add(dp->d_name, lang);
    }
  }
  if (dir) closedir(dir);
  qsort(help_ptr,help_used, sizeof *help_ptr, help_cmp);

#define LANGMASK(_l) ((1<<LANGUAGE_DEFAULT)|(1<<(_l)))
  for(lang = 0; lang < LANGUAGE_COUNT; lang++) {
    fp = xyfopen(num+(FILENAME_HELP_l_index-FILENAME_HELP_l), "w", lang);
    if (!fp) continue;
    cnt=0;
    for(idx=0; idx <help_used; idx++) {
      if (!(help_ptr[idx].mask & LANGMASK(lang))) continue;
      fprintf(fp,"%s\n", help_ptr[idx].name);
      cnt++;
    }
    fclose(fp);
  }
  free(help_ptr);
  help_ptr=NULL;
  help_used=help_size=0;
}

/* AvK this searches the "help.index" file in the language subdir of 
** the dir, indicated by num.
** it is used instead of the search_directory() function
*/
int search_index(char * buff, size_t buffsiz, const char *needle, int num, int lang)
{
  FILE *fp;
  char line[200];
  int cnt=0;
  int pos=0;
  size_t nlen;
  size_t len;

  fp=xyfopen(num, "r", lang);
  if (!fp) return -1;

  nlen = (needle) ? strlen(needle): 0;
  while(fgets(line, sizeof line, fp)) {
    if (nlen && strncmp(line,needle, nlen)) continue;
    len = strlen(line);
    if (len < 1) continue;
    line[len-1]= 0; /* remove \n */
    if (pos + len>= buffsiz) break;
    memcpy(buff+pos,line,len); /* len includes '\0' */
    pos += len;
    cnt++;
  }
  fclose(fp);

  return cnt;
}

