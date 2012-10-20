/*
**   
** emote2.c -- look up strings in config file for formulating messages
** to clients
**
** Copyright 1995 J. Alan Eldridge.	
**
** modification history
** ==================================================
** 1995/01/13 alane@wozzle: created.
** 20010318 Avk modified/rewritten in C
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "emote2.h"
#define UNUSED(_p) (void)(_p)

/* static data */

static char * argstr=0;
static size_t argpos=0;
static size_t arglen=0;
static int argtyp=0;
static char * emote_init_arg(char *str);
static char * emote_next_arg(void);


/* get lines from a config file - the following rules apply

  1. the two-character sequence \# is treated as a literal #.
  2. otherwise, all characters from a # (inclusive) to a newline
     (exclusive) are treated as a comment.
  3. otherwise, a \ immediately followed by a newline is a 
     line-continuation, and is simply ignored.
  4. all other characters are treated literally.
  5a.if a line is read in its entirety, the last character
     in the buffer (before the zero) will be a newline.
  5b.a line that is too long to fit in the buffer will be
     retrieved with multiple calls.     
*/

/* ugets(fp,buf,size) -- get one logical line. may be
   an empty line. returns # of characters read, or EOF
   if end-of-file was reached on the first read.     */

static int ugets(FILE *fp, char *buf, int size)
{
  int ch;        /*   current character from input            */
  int nc=0;     /*   # characters in current input line      */
  int skip=0;   /*   are we skipping to end-of-line?         */
  int nw=0;     /*   # of non-whitespace characters we got   */

  size--;   /*   leave room for zero byte      */

  while ((ch=getc(fp)) != EOF) {
    /* skipping comments?                   */
    if (skip) {
      if (ch == '\n') { skip = 0; ungetc(ch,fp); }
      continue;
    }

    switch (ch) {
    case '#': /* start comment                        */
      skip=1; 
      continue;
    
    case '\n': /* if we hit newline we're done         */
      goto done;

    case '\\': /* line continuation with \-NL?  or an escaped '#' character?  */
      ch=getc(fp);
      switch (ch) {
      case 'n': ch = '\n'; break;
      case '\n': continue;
      case '#': break; /* falltru to ordinary */
      default:
	ungetc(ch,fp); ch = '\\' ;
      }
	/* FALLTRU */
    default: /* ordinary character */
      if (nc < size) {
        buf[nc]=ch; 
        nc++;
        if (!isspace(ch)) { nw++; }
      } else {
        ungetc(ch,fp);
        goto done;
      }
    }
  }
done:
  buf[nc]=0;
  if (!nw) nc=0;
  return nc ? nc : ch==EOF ? EOF : 0;
}

/**************************************************

  utabgets(fp,buf,size) -- get next non-blank
  logical line (may be several physical lines
  joined together with \-NL) from file.

  loop while ugets(fp,buf) returns zero.
  at this point we either (1) have some text
  or (2) have hit EOF.

***************************************************/

static int utabgets(FILE *fp, char *buf, int size)
{
  int nc;

  do {
     nc=ugets(fp,buf,size);
     } while(nc==0);
  return nc;
}

/* helper to find a keyword */

static char *findkey(const char *key)
{
FILE *fp;
size_t len;
int rc;
char *cp=NULL;
char linebuff[ 2000 ];
    
  fp = xyfopen(FILENAME_EMOTE, "r");
  if (!fp) return NULL; 

  len = strlen(key);

  while ((rc=utabgets(fp, linebuff, sizeof linebuff)) > 0) {
    cp = linebuff;
    while (isspace((int)*cp)) { cp++; }
    if (strncmp(cp, key,len)) continue;
    cp += len;
    while (*cp && isspace((int)*cp)) { cp++; }
    break;
  }

  fclose(fp);
  len = strlen(cp);
  if (len) len--;
  if (cp[len] == '\n') cp[len] = 0;
  return statstr_dup(cp,len);
}

void EmoteInit(const char *path)
{
  UNUSED(path);
  /* AvK: do nothing.  */
}

int EmoteLook(const char *key)
{
  return findkey(key) != 0;
}

const char *EmoteMkStr(char *key, char *username, char *args, int isclient)
{
char *fmt;
int idx = 0;
size_t len=0;
char *arg;
char outbuff[ 2000 ];

fmt = findkey(key);

if (!fmt) {
  return NULL;
  }

args=emote_init_arg(args);
while ((outbuff[idx] = *fmt++) ) {
    if (idx >= (int)sizeof outbuff -1) break;
    switch (outbuff[idx]) {
    default:
      idx++; break;
    case '\n':
      idx++;
      if (isclient) {
        if (idx + 4 >= (int)sizeof outbuff -1) goto done;
        strcpy(outbuff+idx, "500 ");
        idx += 4;
        }
      break;
    case  '%':
      switch (*fmt) {
      case 0: goto done;
      default: arg=NULL; break;
      case 'u':
      case 'U' : arg = username; len=strlen(username); break;
      case 's': arg = args; len=arglen; args=emote_next_arg(); break;
	}
      if (!arg) {idx++; continue; }
      fmt++;
      if (idx + len >= sizeof outbuff -1) { goto done; }
      memcpy(outbuff+idx,arg, len);
      idx+=len;
      break;
    }
  }
done:
  return statstr_dup(outbuff,idx);
}

static char * emote_init_arg(char *str)
{
argstr = str;
argpos = 0;
arglen = 0;
return emote_next_arg();
}

static char * emote_next_arg(void)
{

  for(argpos+=arglen ; argstr[argpos]; argpos++) {
    if (argstr[argpos] == ' ') continue;
    if (argstr[argpos] == '\t') continue;
    break;
    }

  switch(argstr[argpos]) {
  case '"' : case '\'' : argtyp = argstr[argpos]; break;
  default: argtyp = 0; break;
  }

  if (argtyp) {
    argpos++;
    for(arglen=argpos; argstr[arglen]; arglen++) {
      if (argstr[arglen] == argtyp) break;
      }
  } else {
    for(arglen=argpos; argstr[arglen]; arglen++) {
      if (argstr[arglen] == ' ') break;
      if (argstr[arglen] == '\t') break;
    }
  }
  arglen -= argpos;
  return argstr+argpos;
}

/*#define WANT_MAIN*/
#if WANT_MAIN

int main(int, const char **)
{
  EmoteInit("./emotestr");

  const char *psz = EmoteMkStr("wanker", "alane", "'Newt Gingrich' pathetic", 1);

  if (psz) {
    puts(psz);
    psz = EmoteMkStr("wanker", "alane", "'Newt Gingrich'", 0);
    puts(psz);
  } else {
    puts("No wanker found. Odd, that.");
  }

  return 0;
}

#endif /* TESTMAIN */

