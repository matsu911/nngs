/* utils.c
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
#include <stdarg.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

int ftruncate(int fd, off_t size);
int truncate(const char *path, off_t size);

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef SGI
#include <sex.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "missing.h"
#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "utils.h"
#include "common.h"
#include "command.h"
#include "servercodes.h"
#include "playerdb.h"
#include "multicol.h"
#include "network.h"
#include "language.h"
#include "mailer.h"


struct searchdata mysearchdata;
static char filename1[MAX_FILENAME_SIZE];
static char filename2[MAX_FILENAME_SIZE];

/* This is ugly: to avoid dynamic memory allocation,
 * we create a pool where 'temporary' strings are put.
 * This memory is never 'free()'d, but recycled in a round-robin-fashion.
 * This is introduced to avoid functions to return 
 * a pointer to their own static char buff[]'s.
 * The functions statstr_get() and statstr_dup()
 * return pointers into this pool.
 */
static char statstr_buff[16 * 1024] ;
static size_t statstr_top = 0;

static int pcvprintf(int p, int code, const char *format, va_list ap);

static int mkdir_p(const char * pathname, int mode);
static int pprompt(int p0);
static size_t vafilename(char *buf,int num, va_list ap);
static int lines_file(char *file);
static FILE * vafopen(int num, const char * mode, va_list ap);
static FILE * pvafopen(int p, int num, const char * mode, va_list ap);


extern int vfprintf(FILE *fp, const char *fmt, va_list ap);
extern int snprintf(char *dst, size_t dstlen, const char *fmt, ...);
extern int vsnprintf(char *dst, size_t dstlen, const char *fmt, va_list ap);
extern FILE * popen(const char *cmd, const char *mode);
extern int pclose(FILE *fp);
extern char *tempnam(const char *dir, const char *pfx);

int iswhitespace(int c)
{
#if 0
  if (c < ' ' || c == '\b' || c == '\n' ||
      c == '\t' || c == ' ') {	/* white */
    return 1;
  } else {
    return 0;
  }
#else
  /* PEM's whitespace. Note that c may be < 0 for 8-bit chars. */
  /* Another note:  Some code depend on c == '\0' being whitespace. /pem */
  return ((0 <= c && c <= ' ') || c == 127);
#endif
}

char *KillTrailWhiteSpace(char *str)
{
  size_t len;

  for( len = strlen(str); len; len--) {
    if (!iswhitespace(str[len-1])) break;
    str[len-1] = '\0';
  }
  return str;
}

char *getword(char *str)
{
  int len;
  char word[MAX_WORD_SIZE];

  for( len = 0; !iswhitespace(str[len] ); len++) {;}
  
  return statstr_dup(str, len);
}

/* This code defines the TYPE of message that is to be sent by adding
   a prefix to the message. */
const char *SendCode(int p, int Code)
{
  char word[MAX_WORD_SIZE];
  int len=0;

  if (!parray[p].flags.is_client) {
    switch(Code) {
      case CODE_SHOUT:
        strcpy(word, "\n"); len = 1; break;
      default:
        word[0]= 0; break;
    }
  }
  else {
    switch(Code) {
      case CODE_MOVE:
      case CODE_INFO:
        len = sprintf(word, "%d ", Code); break;
      case CODE_SHOUT:
        len = sprintf(word, "\n%d ", Code); break;
      default:
        len = sprintf(word, "%d ", Code); break;
    }
  }
  if (len < 0) len = 0;
  return statstr_dup(word, len);
}

char *eatword(char *str)
{
  while (*str && !iswhitespace(*str))
    str++;
  return str;
}

char *eatwhite(char *str)
{
  while (*str && iswhitespace(*str))
    str++;
  return str;
}

char *nextword(char *str)
{
  return eatwhite(eatword(str));
}

int mail_ast(const char *addr, const char *subj, const char *text)
{
  int ret;
  char nbuf[MAX_FILENAME_SIZE];

  Logit("Mail_ast(%s,%s,%s)", addr, subj, text);
  if (!safestring(addr)) return -1;
  ret = mail_spool(nbuf, addr , subj, text, NULL);
  Logit("Mail_spool(%s) :=%d", nbuf, ret);
  return ret;
}

int mail_pst(int p, const char *subj, char *text)
{

  Logit("Mail_pst(%d,%s,%s)", p, subj, text);
  if (!parray[p].email[0]) return -1;
  if (!subj) subj = "NNGS game report";
  return mail_ast(parray[p].email, subj, text);
}

int mail_psn(int p, const char *subj, const char *fname)
{

  Logit("Mail_psn(%d,%s,%s)", p, subj, fname);

  if (!parray[p].email[0] || !safestring(parray[p].email)) return -1;
  return mail_asn(parray[p].email, subj, fname);
}

int mail_asn(const char *addr, const char *subj, const char *fname)
{
  char nbuf[MAX_FILENAME_SIZE];
  int ret;

  ret = 0;

  Logit("Mail_asn(%s,%s,%s)", addr, subj, fname);
  if (!addr || !*addr || !safestring(addr)) return -1;

  ret = mail_spool(nbuf, addr , subj, NULL, fname);
 
  return ret;
}
/* Process a command, imposing as another user */
int pcommand(int p, const char *comstr, ...)
{
  va_list ap;
  char buff[MAX_LINE_SIZE];
  int retval;
  int fd = parray[p].session.socket;

  va_start(ap, comstr);
  my_vsnprintf(buff, sizeof buff, comstr, ap);
  va_end(ap);

  retval = process_input(fd, buff);
  if (retval == COM_LOGOUT) {
    process_disconnection(fd);
    /* net_close(fd); */
  }
  return retval;
}

int Logit(const char *format,...)
{
  va_list ap;
  FILE *fp;
  char fname[MAX_FILENAME_SIZE];
  char textbuff[10 * MAX_LINE_SIZE];	/* Make sure you can handle 10 lines worth of
				   stuff */
  int retval;
  time_t time_in;
  static int in_logit = 0;

#if SKIPPIT
  return 0;
#endif

  time_in = globclock.time;
  va_start(ap, format);

  retval = vsprintf(textbuff, format, ap);
  va_end(ap);
  if (strlen(textbuff) >= sizeof textbuff) {
    fprintf(stderr, "Logit buffer overflow (format=\"%s\")\n", format);
    textbuff[sizeof textbuff -1] = 0;
  }

  sprintf(fname, "%s", conffile.log_file);
  
	/* Terminate the mutual recursion Logit() <--> xfopen()
	** (this can only occur if the logfile does not exist)
	*/
  if (in_logit++) {
     /* fprintf(stderr, "In_logit(recursion(%d)) '%s'\n",in_logit, fname); */
  } else if (!(fp = xfopen(fname, "a"))) {
     int err;
     err = errno;
     fprintf(stderr, "Error opening logfile '%s': %d(%s)\n"
       ,fname, err, strerror(err) );
  } else  {
      size_t len;
      char timebuff[30];
      strcpy ( timebuff, asctime(localtime(&time_in)));
      len = strlen(timebuff);
      if (len) timebuff[len-1] = 0;
    if (conffile.want_new_log_style > 0) {
      fprintf(fp, "%s %s\n", timebuff, textbuff);
    } else {
      fprintf(fp, "%s %s\n", textbuff, timebuff );
    }
    fflush (fp);
    fclose(fp);
  }
  in_logit--;
  return retval;
}


int pprintf(int p, const char *format, ...)
{
  va_list ap;
  char buff[10 * MAX_LINE_SIZE];	/* Make sure you can handle 10 lines worth of
				   stuff */
  int retval;
  size_t len;
  va_start(ap, format);
	/* AvK: the strlen can be avoided if we can trust stdlib.
	** (the printf() family _is_supposed_ to return 
	** the strlen() of the resulting string, or -1 on error )
	*/
  retval = vsprintf(buff, format, ap);
  va_end(ap);
#if 0
  retval = strlen(buff);
#endif
  len = retval;
  if (retval < 0 || (unsigned) retval >= sizeof buff) {
    Logit("pprintf buffer overflow: %d > %u", retval, (unsigned) sizeof buff);
    len = sizeof buff -1; buff[len] = 0;
  }
  net_send(parray[p].session.socket, buff, len);
  return retval; /* AvK: should be equal to len, but is always ignored anyway */
}

static int pcvprintf(int p, int code, const char *format, va_list ap)
{
  char buff[10 * MAX_LINE_SIZE];

  int rc, len;
  int idx = 0;

#if SUPPRESS_SYMPTOMS /* :-) */
  memset(buff,0,sizeof buff);
#endif

	/* A leading \n can be printed *before* the sendcode */
  if (code & CODE_CR1) {
    code &= ~CODE_CR1;
    idx = sprintf(buff,"\n");
  }
	/* CODE_NONE == 0: Dont send code or propt */
  if (code) {
    const char *cp;
    cp = SendCode(p, code);
    len = strlen(cp);
    memcpy(buff+idx,cp,len);
    idx += len;
/* debugging info
    fprintf(stderr,"format='%s'",format);
    fprintf(stderr,"cp='%s' idx=%d buff='%s'\n",cp,idx,buff);
*/
  }

  rc = my_vsnprintf(buff+idx,sizeof buff -idx, format, ap);

  if (rc < 0) {
    Logit("pcvprintf buffer overflow code==%d, format==\"%s\"",code,format);
    len = sizeof buff -1; buff[len] = 0;
    strcpy(NULL, "myabort()" );
  }
  else len = idx+rc;

  net_send(parray[p].session.socket, buff, len);

  return len;
}

/*
 * Wrappers around [v]snprintf().
 * They only differ in the returnvalue:
 * If the formatted string does not fit into dstlen, [v]snprintf()
 * does not write beyond dst[dstlen-1], but it will return > dstlen.
 * These my_xxxx functions return <0 on EVERY error.
 */
int my_vsnprintf(char *dst, size_t dstlen, const char *format, va_list ap)
{
  int rc;

#if (HAVE_VSNPRINTF)
  rc = vsnprintf(dst, dstlen, format, ap);
#else
  rc = fake_vsnprintf(dst, dstlen, format, ap);
#endif
  if (rc >= (int) dstlen) rc = -1;
  return rc;
}

int my_snprintf(char *dst, size_t siz, const char *format, ... )
{
  va_list ap;
  int rc;

  va_start(ap, format);
  rc = my_vsnprintf(dst, siz, format, ap);
  va_end(ap);

  return rc;
}

#if (1 || !HAVE_VSNPRINTF || !HAVE_SNPRINTF)
/* this is a simple, robust (and clumsy ...)
 * substitution for the [v]snprintf() functions, which
 * still seem to be absent on some systems.
 * We use a temp file, which cannot cause any buffer-overrun.
 * The trick of unlinking the file immediately after creation
 * is, of course, a unixism. Other platforms may lose.
 * We keep the file/inode open all the time and rewind it each
 * time it is used.
 *
 * Don't complain about performance, instead upgrade your libc.
 * A better, but very big implementation can be found in the
 * Apache sources.
 */
int fake_vsnprintf(char *dst, size_t dstlen, const char *format, va_list ap)
{
  static FILE * dummy = NULL;
  int rlen, wlen;


  if (!dummy) {
    char *name;
    name = tempnam("/tmp/" , "nngs_");
    if (!name) name = mystrdup( "/tmp/vsnprintf.tmp") ;
    dummy = fopen(name, "w+");
    if (!dummy) {
      fprintf(stderr, "Could not open tempfile '%s'", name);
      name+=5; /* skip /tmp/ */
      dummy = fopen(name, "w+");
    }
    if (!dummy) fprintf(stderr, "Could not open tempfile '%s'", name);
    else fprintf(stderr, "Opened tempfile(%d) '%s'", fileno(dummy), name);
#if 1
    unlink(name);
    free(name);
#endif
  }
  rewind(dummy);
  wlen = vfprintf(dummy, format, ap);
  fflush(dummy);
  if (wlen < 0 || wlen >= (int) dstlen) { memcpy(dst,"Badw!",6); return -1; }
  if (wlen < 0 || wlen >= (int) dstlen) { *dst = 0; return -1; }
  rewind(dummy);
  rlen = fread(dst, 1, (size_t) wlen, dummy);
  if (rlen != wlen && dstlen >= 6) { memcpy(dst, "BadR!", 6) ; return -1; }
  if (rlen != wlen) { *dst = 0; return -1; }
  dst[rlen] = 0;
  return rlen;
}

#endif

int cpprintf(int p, int code, const char *format, ...)
{
  va_list ap;
  int retval;

  va_start(ap, format);

  retval = pcvprintf(p, code, format, ap);

  va_end(ap);
  return retval;
}


int pprintf_prompt(int p, const char *format,...)
{
  va_list ap;
  char buff[10 * MAX_LINE_SIZE];	/* Make sure you can handle 10 lines worth of
				   stuff */
  int retval;
  size_t len;

  va_start(ap, format);

  retval = vsprintf(buff, format, ap);
  if ((len = strlen(buff)) >= sizeof buff) {
    Logit("pprintf_prompt buffer overflow");
    len = sizeof buff -1; buff[len] = 0;
  }
  net_send(parray[p].session.socket, buff, len);

  pprompt(p);
  va_end(ap);
  return retval;
}

int cpprintf_prompt(int p, int code, const char *format,...)
{
  va_list ap;
  int retval;

  va_start(ap, format);

  retval = pcvprintf(p, code, format, ap);

  pprompt(p);

  va_end(ap);
  return retval;
}

static int pprompt(int p0)
{
  char buff[MAX_LINE_SIZE];
  int len = 0;

  if (parray[p0].flags.is_client) {
    len = sprintf(buff, "%d %d\n", CODE_PROMPT, parray[p0].session.protostate);
  }
  else if (parray[p0].session.protostate == STAT_SCORING) {
    len = sprintf(buff,"Enter Dead Group: "); 
  } else {
    if (parray[p0].extprompt) {
      len = sprintf(buff, "|%s/%d%s| %s ", 
        parray[p0].forget.last_tell >= 0 ? parray[parray[p0].forget.last_tell].pname : "",
        parray[p0].last_channel, 
        parray[p0].busy[0] ? "(B)" : "",
        parray[p0].prompt);
    }
    else len = sprintf(buff, "%s", parray[p0].prompt);
  }
 if (len>0) net_send(parray[p0].session.socket, buff, len);
  return len;
}


static int
is_regfile(char *path)
{
  struct stat sbuf;

  if (stat(path, &sbuf) < 0)
    return 0;
  else
    return S_ISREG(sbuf.st_mode);
}

int psend_raw_file(int p, const char *dir, const char *file)
{
  FILE *fp;
  char buff[MAX_LINE_SIZE * 2];
  char fname[MAX_FILENAME_SIZE];
  int num;

  if (dir) sprintf(fname, "%s/%s", dir, file);
  else     strcpy(fname, file);

  if (!is_regfile(fname)) return -1;

  fp = xfopen(fname, "r");
  if (!fp)
  {
    fprintf(stderr,"psend_raw_file: File '%s' not found!\n",fname);
    return -1;
  }
  while ((num = fread(buff, sizeof(char), sizeof buff, fp)) > 0) {
    net_send(parray[p].session.socket, buff, num);
  }
  fclose(fp);
  return 0;
}

int psend_file(int p, const char *dir, const char *file)
{
  FILE *fp;
  char buff[MAX_LINE_SIZE * 2];
  char fname[MAX_FILENAME_SIZE];
  int lcount = 1;
  char *cp;

  parray[p].forget.last_file[0] = '\0';
  parray[p].forget.last_file_line = 0;  

  if (dir) sprintf(fname, "%s/%s", dir, file);
  else     strcpy(fname, file);
  fp = xfopen(fname, "r");
  if (!fp)
  {
    fprintf(stderr,"psend_file: File '%s' not found!\n",fname);
    return -1;
  }

  if (conffile.debug_general) Logit("Opened \"%s\"", fname);
  if (parray[p].flags.is_client) pcn_out(p, CODE_HELP, FORMAT_FILEn);
  while ((cp = fgets( buff, sizeof buff, fp))) {
    if (lcount >= (parray[p].d_height-1)) break;
    net_sendStr(parray[p].session.socket, buff);
    lcount++;
  }
  if (cp) {
    do_copy(parray[p].forget.last_file, fname, sizeof parray[0].forget.last_file);
    parray[p].forget.last_file_line = (parray[p].d_height-1);
    if (parray[p].flags.is_client) pcn_out(p, CODE_HELP, FORMAT_FILEn);
    pcn_out(p, CODE_INFO, FORMAT_TYPE_OR_qNEXTq_TO_SEE_NEXT_PAGE_n);
  }
  else {
    if (parray[p].flags.is_client) pcn_out(p, CODE_HELP, FORMAT_FILEn);
    }
  fclose(fp);
  return 0;
}

int pxysend_raw_file(int p, int num, ...)
{
  va_list ap;
  FILE *fp;
  int cnt;
  char buff[MAX_LINE_SIZE * 2];
  va_start(ap, num);

  fp = pvafopen(p, num, "r", ap);

  if (!fp)
  {
    Logit("pxysend_raw_file: File '%s' not found!\n",filename1);
    return -1;
  }

  if (!is_regfile(filename1)) { fclose(fp); return -1; }

  while ((cnt = fread(buff, 1, sizeof buff, fp)) > 0) {
    net_send(parray[p].session.socket, buff, cnt);
  }
  fclose(fp);
  return 0;
}

int pxysend_file(int p, int num, ...)
{
  va_list ap;
  FILE *fp;
  int rc = 0;

  va_start(ap, num);

  parray[p].forget.last_file[0] = '\0';
  parray[p].forget.last_file_line = 0;  

  fp = pvafopen(p, num, "r", ap);
  if (!fp)
  {
    Logit("pxysend_file: File '%s' not found!\n",filename() );
    va_end(ap);
    return -1;
  }
  do_copy(parray[p].forget.last_file, filename(), sizeof parray[0].forget.last_file);

  fclose(fp);
  rc = pmore_file( p );
  va_end(ap);
  return rc;
}

int pmore_file( int p )
{  
  FILE *fp;
  char buff[MAX_LINE_SIZE * 2];
  int lcount = 1;
  char *cp;

  if (!parray[p].forget.last_file[0]) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_MORE_n);
    return -1;
  }  
  
  fp = xfopen(parray[p].forget.last_file, "r" );
  if (!fp) {
    pcn_out(p, CODE_ERROR, FORMAT_FILE_NOT_FOUND_n);
    return -1;
  }
  
  if (parray[p].flags.is_client) {
  pcn_out(p, CODE_HELP, FORMAT_FILEn);
  }
  while((cp = fgets(buff, sizeof buff, fp))) {
    if (lcount >= (parray[p].forget.last_file_line + parray[p].d_height-1)) break;
    if (lcount >= parray[p].forget.last_file_line) 
      net_sendStr(parray[p].session.socket, buff);
    lcount++;
  }
  if (cp) {
    parray[p].forget.last_file_line += parray[p].d_height-1;
    if (parray[p].flags.is_client) {
    pcn_out(p, CODE_HELP, FORMAT_FILEn);
    }
    pcn_out(p, CODE_INFO, FORMAT_TYPE_qNEXTq_OR_TO_SEE_NEXT_PAGE_n);
  }
  else {
    parray[p].forget.last_file[0] = '\0';
    parray[p].forget.last_file_line = 0;
    if (parray[p].flags.is_client) {
    pcn_out(p, CODE_HELP, FORMAT_FILEn);
    }
  }
  fclose(fp);
  return 0;
}

int xpsend_dir(int p, int num)
{
  int cnt;
  DIR *dp;
  struct dirent *ep;


  dp = xyopendir(num, p);

  Logit("xpsend_dir(%d,%d):%p", p, num, (void*)dp);

  if (!dp) {
    Logit("diropen failed: %d:%s", errno, strerror(errno) );
    return -1;
  }
  for (cnt = 0; (ep = readdir(dp)); ) {
    if (ep->d_name[0] == '.') continue;
    pprintf(p, "%s%c", ep->d_name,  (++cnt % 4)  ? '\t': '\n' );
  }
  if (cnt % 4) pprintf(p, "\n" );
  closedir(dp);
  return cnt;
}

char *stoupper(char *str)
{
  int i;

  if (!str)
    return NULL;
  for (i = 0; str[i]; i++) {
    if (islower((int)str[i])) {
      str[i] = toupper(str[i]);
    }
  }
  return str;
}

char *stolower(char *str)
{
  int i;

  if (!str)
    return NULL;
  for (i = 0; str[i]; i++) {
    if (isupper((int)str[i])) {
      str[i] = tolower(str[i]);
    }
  }
  return str;
}

static char unsafechars[] = ">!&*?/<|`$;()[]" ;
int safechar(int c)
{
#if 0
  if ((c == '>') || (c == '!') || (c == '&') || (c == '*') || (c == '?') ||
      (c == '/') || (c == '<') || (c == '|') || (c == '`') || (c == '$') ||
      (c == ';') || (c == '(') || (c == ')') || (c == '[') || (c == ']'))
#else
  if (strchr(unsafechars, c))
#endif
    return 0;
  return 1;
}

int safestring(const char *str)
{

  if (!str)
    return 1;
#if 0
  for ( ; *str; str++) {
    if (!safechar(*str))
      return 0;
  }
#else
  if (str[strcspn(str,unsafechars)])
    return 0;
  else return 1;
#endif
  return 1;
}

/* [PEM]: Don't allow dots and slashes in filenames. */
int safefilename(const char *path)
{
  if (!safestring(path))
    return 0;
  for (	;*path; path++) {
    if (*path == '.' || *path == '/') return 0;
    if (*path == '%' || *path == '\\') return 0; /* Dont want these. AvK */
  }
  return 1;
}

/* [PEM]: This is used to check player names when registering.
** Allowing '-' breaks file_[bw]player() below. Possibly other
** strange characters can break things too.
** The new version allows names of the regexp: [A-Za-z]+[A-Za-z0-9]*
*/
int invalid_pname(const char *str)
{
  /* [PEM]: Seemed easiest to rewrite it. */
  if (!str || !isalpha((int)*str))
    return 1;
  while (*++str)
    if (!isalnum((int)*str))
      return 1;
  return 0;
}

int printablestring(const char *str)
{

  if (!str) return 1;

  for (; *str; str++) {
    if (!isprint((int)*str) && *str != '\t' && *str != '\n') return 0;
  }
  return 1;
}

char * mystrdup(const char *str)
{
  char *new;
  size_t len;

  if (!str) {
    Logit("Attempt to strdup a NULL string");
    return NULL;
  }
  len = strlen(str);
  new = malloc(len + 1);
  memcpy(new, str, len); new[len] = 0;
  return new;
}

char *secs2str_short(int t)
{
  char tstr[20];
  int len;
  int h, m, s;

  h = t / 3600;
  t = t % 3600;
  m = t / 60;
  s = t % 60;
  if (h > 99) h = 99;
  if (h) len = sprintf(tstr, "%dh", h);
  else if (m) len = sprintf(tstr, "%dm", m);
  else len = sprintf(tstr, "%ds", s);
  if (len < 0) len = 0;
  return statstr_dup(tstr, len);
}

char *secs2hms_long(int t)
{
  char tstr[60];
  int len, h, m, s;

  h = t / 3600;
  t = t % 3600;
  m = t / 60;
  s = t % 60;

  len = sprintf(tstr, "%d hours, %d minutes, %d seconds", h ? h:0, m ? m:0, s ? s:0);
  if (len < 0) len = 0;
  return statstr_dup(tstr, len);
}

char *secs2hms_mask(int t, int mask)
{
  char tstr[20], *this;
  int h, m, s;
  int pos = 0;

  h = t / 3600;
  t = t % 3600;
  m = t / 60;
  s = t % 60;
  if (h && (mask&4)) {
    if (mask&8) pos += sprintf(tstr+pos, "%d :", h);
    else pos += sprintf(tstr+pos, "%d:", h);
  }
  if (mask&2) {
    if (mask&8) pos += sprintf(tstr+pos, " %d", m);
    else pos += sprintf(tstr+pos, "%02d", m);
  }
  if (mask&1) {
    if (mask&8) pos += sprintf(tstr+pos, " : %02d", s);
    else pos += sprintf(tstr+pos, ":%02d", s);
  }
  if (pos <0) pos = 0;
  this = statstr_dup(tstr, pos);
  return this;
}


static const char *dayarray[] =
{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static const char *montharray[] =
{"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug",
"Sep", "Oct", "Nov", "Dec"};

static char *tm2str(struct tm * stm)
{
  char tstr[100];
  int len;

  len = sprintf(tstr, "%s %3.3s %2d %02d:%02d:%02d %4d",
	  dayarray[stm->tm_wday],
	  montharray[stm->tm_mon],
	  stm->tm_mday,
	  stm->tm_hour,
	  stm->tm_min,
          stm->tm_sec,
          stm->tm_year + 1900);
  if (len < 0) len = 0;
  return statstr_dup(tstr, len);
}

char *tm2str_ccyy_mm_dd(const struct tm * stm)
{
  char tstr[16];
  int len;

  len = sprintf(tstr, "%4d-%02d-%02d",
          stm->tm_year + 1900,
          stm->tm_mon + 1,
          stm->tm_mday);
  if (len < 0) len = 0;
  return statstr_dup(tstr, len);
}

char *ResultsDate(char *fdate)
{
  char tstr[12];
  int len;

  len = sprintf(tstr, "%c%c/%c%c/%c%c%c%c", fdate[4], fdate[5], fdate[6], fdate[7],
                                      fdate[0], fdate[1], fdate[2], fdate[3]);

  if (len < 0) len = 0;
  return statstr_dup(tstr, len);
}

char *time2str_file(const time_t * clk)
{
  char tstr[14];
  struct tm *stm = gmtime(clk);
  int len;

  len = sprintf(tstr, "%04d%02d%02d%02d%02d",
          stm->tm_year + 1900,
          stm->tm_mon + 1,
	  stm->tm_mday,
	  stm->tm_hour,
	  stm->tm_min);
  if (len < 0) len = 0;
  return statstr_dup(tstr, len);
}

char *time2str_sgf(const time_t * clk)
{
  struct tm *stm = gmtime(clk);
  return tm2str_ccyy_mm_dd(stm);
}

char *time2str_local(const time_t * clk)
{
  struct tm *stm = localtime(clk);
  return tm2str(stm);
}

char *time2str_utc(const time_t * clk)
{
  struct tm *stm = gmtime(clk);
  return tm2str(stm);
}

/*
 * To supply a uniform "transaction time", which is constant
 * within one iteration of the select() loop, we store the time
 * in the global variable globclock.
 * Both granularities (1sec, 0.1 sec) are maintained.
 * The returnvalue is the number of seconds since the last call.
 * As a side effect, this will reduce the number of systemcalls ...
 */
/* Ticks are used only for relative timeing (game time limits)
 * since it reports seconds since about 5:00pm on Feb 16, 1994
 */

#define NNGS_EPOCH 331939277
struct ticker globclock = {0,0};
unsigned refetch_ticker(void)
{
  struct timeval tp;
  unsigned elapsed;

  gettimeofday(&tp, NULL);
/* .1 seconds since 1970 almost fills a 32 bit int! So lets subtract off
 * the time right now */
  elapsed = tp.tv_sec - globclock.time;
  globclock.time = tp.tv_sec;
  globclock.tick = SECS2TICS(tp.tv_sec - NNGS_EPOCH)
   + tp.tv_usec / (1000000/TICSPERSEC);
  return elapsed;
}

#if 0
unsigned read_tick()
{
  struct timeval tp;
  /* struct timezone tzp; */

  gettimeofday(&tp, NULL);
/* .1 seconds since 1970 almost fills a 32 bit int! So lets subtract off
 * the time right now */
  return SECS2TICS(tp.tv_sec - NNGS_EPOCH)
   + tp.tv_usec / (1000000/TICSPERSEC);
}

/* This is to translate tenths-secs ticks back into 1/1/70 time in full
 * seconds, because vek didn't read utils.c when he programmed new ratings.
   1 sec since 1970 fits into a 32 bit int OK. 
*/
int untenths(unsigned int tenths)
{
  return tenths/10 + NNGS_EPOCH;
}

char *tenth_str(unsigned int t, int spaces)
{
  return secs2hms_mask((t + 5) / 10, 0, 1, spaces);	/* Round it */
}
#endif

#define MAX_TRUNC_SIZE 100


/* Warning, if lines in the file are greater than 1024 bytes in length, this
   won't work! */
int truncate_file(char *file, int lines)
{
  FILE *fp;
  int bptr = 0, ftrunc = 0, i;
  char tBuf[MAX_TRUNC_SIZE][MAX_LINE_SIZE];
  char *cp;
  size_t len;

  if (lines > MAX_TRUNC_SIZE)
    lines = MAX_TRUNC_SIZE;
  fp = xfopen(file, "r+");
  if (!fp)
  {
    fprintf(stderr,"truncate_file: File '%s' not found!\n",file);
    return 1;
  }
  if (conffile.debug_general) Logit("Opened %s", file);
  while ((cp = fgets(tBuf[bptr], MAX_LINE_SIZE, fp))) {
    len = strlen(cp); if (len < 1) continue;
    if (tBuf[bptr][len-1] != '\n') {	/* Line too long */
      fclose(fp);
      return -1;
    }
    bptr++;
    if (bptr == lines) {
      ftrunc = 1;
      bptr = 0;
    }
  }
  if (ftrunc) {
    fseek(fp, 0, SEEK_SET);
    ftruncate(fileno(fp), 0);
    for (i = 0; i < lines; i++) {
      fputs(tBuf[bptr], fp);
      bptr++;
      if (bptr == lines) {
	bptr = 0;
      }
    }
  }
  fclose(fp);
  return 0;
}


#if NEWTRUNC
int truncate_file(char *file, int lines)
{
  FILE *fp;
  int bptr = 0, trunc = 0, i;
  char tBuf[MAX_TRUNC_SIZE+10][MAX_LINE_SIZE];

  if (lines > MAX_TRUNC_SIZE)
    lines = MAX_TRUNC_SIZE;
  fp = xfopen(file, "r");
  if (!fp)
  {
    fprintf(stderr,"truncate_file: File '%s' not found!\n",file);
    return 1;
  }
  if (conffile.debug_general) Logit("Opened %s", file);
  while (fgets(tBuf[bptr], MAX_LINE_SIZE, fp)) {
    if (tBuf[bptr][strlen(tBuf[bptr]) - 1] != '\n') {
      fclose(fp);
      return -1;
    }
    bptr++;
    if (bptr == lines) {
      trunc = 1;
    }
  }
  if (trunc) {
    fclose(fp);
    fp = xfopen(file, "w");    
    for (i = bptr-lines; i < bptr; i++) {
      fputs(tBuf[i], fp);
    }
  }
  fclose(fp);
  return 0;
} 
#endif

int xylines_file(int num,...)
{
  va_list ap;
  int cnt;

  va_start(ap, num);

  memset(filename1,0,sizeof filename1);
  vafilename(filename1,num, ap);
  cnt = lines_file(filename1);

  va_end(ap);
  return cnt;
}
/* Warning, if lines in the file are greater than 1024 bytes in length, this
   won't work! */
static int lines_file(char *file)
{
  FILE *fp;
  int lcount = 0;
  char new[MAX_LINE_SIZE];

  fp = xfopen(file, "r");
  if (!fp)
    return 0;
  while (fgets(new, sizeof new, fp))
    lcount++;

  fclose(fp);
  return lcount;
}

int file_has_pname(const char *fname, const char *plogin)
{
  if (!strstr(fname, plogin)) return 0;
  if (!strcmp(file_wplayer(fname), plogin))
    return 2;
  if (!strcmp(file_bplayer(fname), plogin))
    return 1;
  return 0;
}

char * statstr_new(size_t len)
{
  char *this;

  if (statstr_top + len > sizeof statstr_buff) statstr_top = 0;
  this = statstr_buff + statstr_top;
  statstr_top += len;
  return this;
}

char * statstr_dup(const char *str, size_t len)
{
  char *new;

  if (!len) len = strlen(str);
  new = statstr_new(len+1);
  memcpy (new, str, len);
  new[len] = 0;
  return new;
}

char * statstr_trim(const char *str, size_t len)
{
  size_t ltrim, end;

  if (!len) len = strlen(str);
  ltrim = strspn(str, " \t\r\n");
  str += ltrim, len -= ltrim;
  for (end=len; end > 0; end-- ) {
	if (str[end-1] == ' ') continue;
	if (str[end-1] == '\t') continue;
	if (str[end-1] == '\n') continue;
	if (str[end-1] == '\r') continue;
	break;
	}
  return  statstr_dup(*str, end);
}

/* Functions to extract the white and black names from a string
 * "/optiona/path/leading/to/whitename-blackname-optional-date"
 */
const char *file_wplayer(const char *fname)
{
  const char *ptr;
  size_t len;

	/* skip leading directory part */
  ptr = strrchr(fname, '/');
  if (!ptr) ptr = fname;
  else ptr++;
  len = strcspn(ptr, "-");
  return statstr_dup(ptr, len);
}


const char *file_bplayer(const char *fname)
{
  const char *ptr;
  size_t len;

	/* skip leading directory part */
  ptr = strrchr(fname, '/');
  if (!ptr) ptr = fname;
  else ptr++;
  ptr = strrchr(ptr, '-');
  if (ptr) len = strlen(ptr);
  else len = 0;
  return statstr_dup(ptr, len);
}

#if HAVE_ENDIAN_H
#include <endian.h>
#endif
char *dotQuad(unsigned int a)
{
  char buff[20];
  int len;

#if !(BYTE_ORDER ==LITTLE_ENDIAN)
  len = sprintf(buff, "%d.%d.%d.%d"
	, (a & 0xff)
	, (a & 0xff00) >> 8
	, (a & 0xff0000) >> 16
	, (a & 0xff000000) >> 24
	);
#else
  len = sprintf(buff, "%d.%d.%d.%d"
	, (a & 0xff000000) >> 24
	, (a & 0xff0000) >> 16
	, (a & 0xff00) >> 8
	, (a & 0xff)
	);
#endif
  if (len < 0) len = 0;
  return statstr_dup(buff,len);
}

#if OBSOLETE_SOURCE
int available_space(void)
{
#if defined(__linux__)
#include <sys/vfs.h>
  int rc;
  struct statfs buf;

  rc =statfs(conffile.player_dir, &buf);
  return (rc)? 0: ((buf.f_bsize/256) * (buf.f_bavail/4));
#elif defined(SYSTEM_NEXT)
  struct statfs buf;

  statfs(conffile.player_dir, &buf);
  return ((buf.f_bsize/ 256) * (buf.f_bavail/4));
#elif defined(SYSTEM_ULTRIX)
  struct fs_data buf;

  statfs(conffile.player_dir, &buf);
  return ((buf.bfreen));
#else
   return 100000000;		/* Infinite space */
#endif
}

int file_exists(char *fname)
{
  FILE *fp;

  fp = xfopen(fname, "r");
  if (!fp)
    return 0;
  fclose(fp);
  return 1;
}
#endif /* OBSOLETE_SOURCE */

/* read a directory into memory. Strings are stored successively in buff */
/* returns -1 on error, or a count of strings stored */
/* A filter can be passed too */
int search_directory(char *buff, int bufsiz, char *filter, int num, ...)
{
  va_list ap;
  int cnt = 0;
  int pos = 0;
  int filtlen;
  int len;
  int diff;
  DIR * dp;
  struct dirent *ep;

  va_start(ap, num);

/**
  dp = xyopendir(num, p);
**/
  vafilename(filename1,num, ap);
  dp = opendir(filename1);

  va_end(ap);

  Logit("Search_directory(%d):%s", num, (char*)filename1);

  if (!dp) {
    Logit("diropen failed: %d:%s", errno, strerror(errno) );
    return -1;
  }

  filtlen = strlen(filter);
  for (pos = cnt = 0; (ep = readdir(dp)); ) {
    if (ep->d_name[0] == '.') continue;
    len = 1+strlen(ep->d_name);
    if (pos + len >= bufsiz) { break; }
    diff = filter ? strncmp(filter, ep->d_name, filtlen) :0;
    if (diff) continue;
    memcpy(buff+pos, ep->d_name, len);
    pos += len;
    cnt++;
  }
  return cnt;
}

int display_directory(int p, const char *buff, int cnt)
/* buff contains 'cnt' 0-terminated strings in succession. */
{
#define MAX_DISP 800		/* max. no. filenames to display */

  const char *s = buff;
  struct multicol *m = multicol_start(MAX_DISP);
  int i;

  for (i = 0; (i < cnt && i < MAX_DISP); i++) {
    multicol_store(m, s);
    s += strlen(s) + 1;
  }
  multicol_pprint(m, p, 78, 1);
  multicol_end(m);
  return i;
}


void bldsearchdata(char *psz)
{
  if (psz[0] == '-') {
    psz++;
    mysearchdata.where = SDATA_HOST;
  } else {
    mysearchdata.where = SDATA_USER;
  }
  bmInit(strlwr(psz), &mysearchdata.bmData);
}

char *strlwr(char *psz)
{
  char *ret = psz;
  while (*psz) {
    *psz = tolower(*psz);
    psz++;
  }
  return ret;
}

const struct searchresult *search(char *psz)
{
  static struct searchresult sr;
  char *pcStr = 0;
  char *pcTmp = strchr(psz, ':');

  if (!pcTmp)			/* PEM */
    return NULL;
  *pcTmp = 0;
  strcpy(sr.szPlayer,pcTmp + 1);
  strcpy(sr.szMailAddr, psz);

  pcTmp = strchr(psz, '@');
  if (!pcTmp)			/* PEM */
    return NULL;
  *pcTmp = 0;

  if (mysearchdata.where == SDATA_HOST) {
    psz = pcTmp + 1;
  }

  pcStr = bmSrch(strlwr(psz), &mysearchdata.bmData);

  if (pcStr) {
    return &sr;
  }

  return NULL;
}

int is_totally_blank(char *str)
{
  while (*str) {
    if (!isspace((int)*str)) {
      return 0;
    } else {
      str++;
    }
  }
  return 1;
}


size_t do_copy(char *dest, const char *src, size_t maxlen)
{
  /* AvK: changed semantics: maxlen is the declared/allocated
  ** size for dest[] , so dest[maxlen-1] is the last element
  ** and will *always* be set to '\0' */

  size_t len;

  len = strlen(src);
  if (len >= maxlen) {
    Logit("Attempt to copy large string %s (len = %d, maxlen = %d)", src, len, maxlen);
    len = maxlen -1;
  }

  if (len) memcpy(dest, src, len);
  dest[len] = '\0';
  return len;
}

/* AvK: this function is a wrapper around fopen.
** If mode is not read and a directory does not exists,
** it will create the entire path to the file (if allowed).
** This feature is only intended as an aid for testing, 
** because getting all the paths right takes a lot of time.
**
** In a "production" server all the directories should exist.
** Behaviour depends on the global variable mode_for_dirs:
** when zero, no directories are created.
** The function writes to the logfile, if
** *) file not found && writemode.
** *) for each directory it creates.
*/
FILE * xfopen(const char * name, const char * mode)
{
  FILE *fp;
  int err;

  while (1) {
    fp = fopen(name,mode);
    if (fp) break;
    err = errno;

/* Don't log missing files when mode is read.
** (unregistered players, missing messagefiles, etc)
*/
    switch (*mode) {
    case 'r' :
      if (conffile.log_missing_files_on_read < 1) break;
    default:
      Logit("Xfopen: fopen(\"%s\", mode=\"%s\") failed: %d (%s)"
	  , name, mode
	  , err, strerror(err) );
      break;
    }

    if (*mode == 'r') err = 0;

    switch(err) {
    case ENOENT:
      if (conffile.mode_for_dir) {
        err = mkdir_p(name, conffile.mode_for_dir);
        if (err> 0) continue;
	}
      else {;} /*FALLTRU*/
    case 0:
    default:
      goto quit;
    }
  }

quit:
  return fp;
}


FILE * xyfopen(int num, const char * mode, ...)
{
  va_list ap;
  FILE *fp;

  va_start(ap, mode);

  fp = vafopen(num, mode, ap);

  va_end(ap);
  return fp;
}


static FILE * vafopen(int num, const char * mode, va_list ap)
{
  FILE *fp;

  memset(filename1,0,sizeof filename1);
  vafilename(filename1,num, ap);
  fp = xfopen(filename1, mode);

  return fp;
}


FILE * pxyfopen(int p, int num, const char * mode, ...)
{
  va_list ap;
  FILE *fp;
  va_start(ap, mode);

  fp = pvafopen(p, num, mode, ap);

  va_end(ap);
  return fp;
}

static FILE * pvafopen(int p, int num, const char * mode, va_list ap)
{
  FILE *fp;
  const char *pre;
  char *nam;
  int lang;

  switch(num) {
	/* These are the language-dependant directories.
	** Lookup the language, and insert it into the arglist
	** , before the given varargs.
	*/
  case FILENAME_HELP_q:
  case FILENAME_AHELP_q:
    lang = parray[p].language;
    pre = language_num2prefix(lang);
    fp = xyfopen(num+2, mode, pre); /* num+2 := ..HELP_s */
    if (fp) break;
    pre = language_num2prefix(LANGUAGE_DEFAULT);
    fp = xyfopen(num+2, mode, pre);
    break;
  case FILENAME_HELP_q_s:
  case FILENAME_AHELP_q_s:
    nam = va_arg(ap,char*);
    lang = parray[p].language;
    pre = language_num2prefix(lang);
    fp = xyfopen(num+2, mode, pre, nam); /* num+2 := ..HELP_s_s */
    if (fp) break;
    pre = language_num2prefix(LANGUAGE_DEFAULT);
    fp = xyfopen(num+2, mode, pre, nam);
    break;
  default:
    Logit("Pvafopen(%d,%d,...) : not langage-dependent", p,num);
  case FILENAME_MESS_LOGIN:
  case FILENAME_MESS_LOGOUT:
  case FILENAME_MESS_UNREGISTERED:
  case FILENAME_MESS_MOTD:
  case FILENAME_MESS_MOTDs:
  case FILENAME_MESS_AMOTD:
    fp = vafopen(num, mode, ap);
    break;
  }
  return fp;
}

int xyrename(int num1, int num2, ...)
{
  va_list ap1,ap2;
  int rc;

  va_start(ap1, num2);
  va_start(ap2, num2);

  memset(filename1,0,sizeof filename1);
  memset(filename2,0,sizeof filename2);
  vafilename(filename1,num1, ap1);
  vafilename(filename2,num2, ap2);
  rc = rename(filename1, filename2);

  va_end(ap1);
  va_end(ap2);
  return rc;
}

int xyunlink(int num,...)
{
  va_list ap;
  int rc;

  va_start(ap, num);

  memset(filename1,0,sizeof filename1);
  vafilename(filename1,num, ap);
  rc = unlink(filename1);

  va_end(ap);
  return rc;
}

  /* AvK: Sorry, this function uses the current value of the static 
  ** filename1[], so it relies on the caller having called one 
  ** of the other xy...() functions first.
  */
int xylink(int num, ...)
{
  va_list ap;
  int rc;

  va_start(ap, num);

  memset(filename2,0,sizeof filename2);
  vafilename(filename2,num, ap);
  if ((rc = strcmp(filename1, filename2)))
    rc = link(filename1, filename2);

  va_end(ap);
  return rc;
}

int xyfilename(char *buf,int num, ...)
{
  va_list ap;
  int rc;

  va_start(ap, num);

  rc = vafilename(buf,num, ap);

  va_end(ap);
  return rc;
}

static size_t vafilename(char *buf, int num, va_list ap)
{
size_t len;
  const char *cp1, *cp2,*cp3;
  int i1,i2;
#define SUFFIX_LOGONS "logons"
#define SUFFIX_MESSAGES "messages"
#define SUFFIX_GAMES "games"
#define SUFFIX_OLD "old"
#define SUFFIX_DELETE "delete"

  switch(num) {
  case FILENAME_CMDS :
    len = sprintf(buf, "%s/commands", conffile.help_dir);
    break;
  case FILENAME_ACMDS :
    len = sprintf(buf, "%s/admin_commands", conffile.ahelp_dir);
    break;
  case FILENAME_INFO:
    len = sprintf(buf, "%s", conffile.info_dir);
    break;

  case FILENAME_HELP:
    len = sprintf(buf, "%s", conffile.help_dir);
    break;
  case FILENAME_HELP_p:
    i2 = va_arg(ap,int);
    i1 = parray[i2].language;
    goto filename_help_1;
  case FILENAME_HELP_l:
    i1 = va_arg(ap,int);
    goto filename_help_1;
  filename_help_1:
    cp1 = language_num2prefix(i1);
    goto filename_help_0;
  case FILENAME_HELP_s:
    cp1 = va_arg(ap,char*);
    goto filename_help_0;
  filename_help_0:
    len = sprintf(buf, "%s/%s", conffile.help_dir, cp1);
    break;
  case FILENAME_HELP_l_index:
    i1 = va_arg(ap,int);
    cp1 = language_num2prefix(i1);
    goto filename_help_l_index_0;
  case FILENAME_HELP_s_index:
    cp1 = va_arg(ap,char*);
  filename_help_l_index_0:
    len = sprintf(buf, "%s/%s/%s", conffile.help_dir, cp1, ".index" );
    break;
  case FILENAME_HELP_l_s:
    i1 = va_arg(ap,int);
    cp1 = language_num2prefix(i1);
    goto filename_help_l_0;
  case FILENAME_HELP_s_s:
    cp1 = va_arg(ap,char*);
  filename_help_l_0:
    cp2 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s/%s", conffile.help_dir, cp1, cp2);
    break;

  case FILENAME_AHELP:
    len = sprintf(buf, "%s", conffile.ahelp_dir);
    break;
  case FILENAME_AHELP_p:
    i2 = va_arg(ap,int);
    i1 = parray[i2].language;
    goto filename_ahelp_s_1;
  case FILENAME_AHELP_l:
    i1 = va_arg(ap,int);
    goto filename_ahelp_s_1;
  filename_ahelp_s_1:
    cp1 = language_num2prefix(i1);
    goto filename_ahelp_s_0;
  case FILENAME_AHELP_s:
    cp1 = va_arg(ap,char*);
    goto filename_ahelp_s_0;
  filename_ahelp_s_0:
    len = sprintf(buf, "%s/%s", conffile.ahelp_dir, cp1);
    break;
  case FILENAME_AHELP_s_s:
    cp1 = va_arg(ap,char*);
    cp2 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s/%s", conffile.ahelp_dir, cp1, cp2);
    break;
  case FILENAME_AHELP_l_index:
    i1 = va_arg(ap,int);
    cp1 = language_num2prefix(i1);
    goto filename_ahelp_l_index_0;
  case FILENAME_AHELP_s_index:
    cp1 = va_arg(ap,char*);
filename_ahelp_l_index_0:
    len = sprintf(buf, "%s/%s/%s", conffile.ahelp_dir, cp1, ".index" );
    break;

  case FILENAME_PLAYER :
    len = sprintf(buf, "%s", conffile.player_dir);
    break;
  case FILENAME_PLAYER_cs :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s", conffile.player_dir, cp1[0], cp1);
    break;
  case FILENAME_PLAYER_cs_DELETE :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s.%s", conffile.player_dir, cp1[0], cp1, SUFFIX_DELETE);
    break;
  case FILENAME_PLAYER_cs_LOGONS :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s.%s", conffile.player_dir, cp1[0], cp1, SUFFIX_LOGONS);
    break;
  case FILENAME_PLAYER_cs_MESSAGES :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s.%s", conffile.player_dir, cp1[0], cp1, SUFFIX_MESSAGES);
    break;
  case FILENAME_PLAYER_cs_GAMES:
    cp1 = va_arg(ap,char*);
    len = sprintf(buf,"%s/player_data/%c/%s.%s", conffile.stats_dir,cp1[0]
    , cp1, SUFFIX_GAMES);
    break;

  case FILENAME_GAMES_s:
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s" , conffile.game_dir, cp1);
    break;
  case FILENAME_GAMES_c :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c" , conffile.game_dir, cp1[0]);
    break;
  case FILENAME_GAMES_bs_s :
    cp1 = va_arg(ap,char*);
    cp2 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s-%s" , conffile.game_dir, cp2[0], cp1, cp2);
    break;
  case FILENAME_GAMES_ws_s :
    cp1 = va_arg(ap,char*);
    cp2 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s-%s" , conffile.game_dir, cp1[0], cp1, cp2);
    break;

  case FILENAME_CGAMES:
    len = sprintf(buf, "%s" , conffile.cgame_dir );
    break;
  case FILENAME_CGAMES_c:
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c" , conffile.cgame_dir, cp1[0]);
    break;
  case FILENAME_CGAMES_cs:
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s" , conffile.cgame_dir, cp1[0], cp1);
    break;
  case FILENAME_CGAMES_ws_s_s:
    cp1 = va_arg(ap,char*);
    cp2 = va_arg(ap,char*);
    cp3 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s-%s-%s" , conffile.cgame_dir
		, cp1[0], cp1, cp2, cp3);
    break;
  case FILENAME_CGAMES_bs_s_s:
    cp1 = va_arg(ap,char*);
    cp2 = va_arg(ap,char*);
    cp3 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%c/%s-%s-%s" , conffile.cgame_dir
		, cp2[0], cp1, cp2, cp3);
    break;

  case FILENAME_RATINGS :
    len = sprintf(buf,"%s", conffile.ratings_file);
    break;
  case FILENAME_NRATINGS :
    len = sprintf(buf,"%s", conffile.nratings_file);
    break;
  case FILENAME_RESULTS :
    len = sprintf(buf, "%s", conffile.results_file);
    break;
  case FILENAME_NRESULTS :
    len = sprintf(buf, "%s", conffile.nresults_file);
    break;

  case FILENAME_LADDER9 :
    len = sprintf(buf, "%s", conffile.ladder9_file);
    break;
  case FILENAME_LADDER19 :
    len = sprintf(buf, "%s", conffile.ladder19_file);
    break;

  case FILENAME_NEWS_s :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/news.%s", conffile.news_dir, cp1);
    break;
  case FILENAME_NEWSINDEX :
    len = sprintf(buf, "%s/news.index", conffile.news_dir);
    break;
  case FILENAME_ADMINNEWSINDEX :
    len = sprintf(buf, "%s/adminnews.index", conffile.news_dir);
    break;
  case FILENAME_ADMINNEWS_s :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/adminnews.%s", conffile.news_dir, cp1);
    break;
  case FILENAME_NOTEFILE :
    len = sprintf(buf, "%s", conffile.note_file);
    break;
  case FILENAME_LOGONS :
    len = sprintf(buf, "%s", conffile.logons_file);
    break;

  case FILENAME_MESS_LOGIN:
    len = sprintf(buf, "%s/%s", conffile.mess_dir, MESS_LOGIN);
    break;
  case FILENAME_MESS_LOGOUT:
    len = sprintf(buf, "%s/%s", conffile.mess_dir, MESS_LOGOUT);
    break;
  case FILENAME_MESS_WELCOME:
    len = sprintf(buf, "%s/%s", conffile.mess_dir, MESS_WELCOME);
    break;
  case FILENAME_MESS_UNREGISTERED:
    len = sprintf(buf, "%s/%s", conffile.mess_dir, MESS_UNREGISTERED);
    break;
  case FILENAME_MESS_MOTD:
    len = sprintf(buf, "%s/%s", conffile.mess_dir, MESS_MOTD);
    break;
  case FILENAME_MESS_MOTDs:
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s.%s", conffile.mess_dir, MESS_MOTD, cp1);
    break;
  case FILENAME_MESS_AMOTD:
    len = sprintf(buf, "%s/%s", conffile.mess_dir, MESS_AMOTD);
    break;
  case FILENAME_EMOTE:
    len = sprintf(buf, "%s", conffile.emotes_file) ;
    break;

  case FILENAME_FIND :
    len = sprintf(buf, "%s", FIND_FILE);
    break;
  case FILENAME_LISTINDEX :
    len = sprintf(buf, "%s/index", conffile.lists_dir);
    break;
  case FILENAME_LIST_s :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s", conffile.lists_dir, cp1);
    break;
  case FILENAME_LIST_s_OLD :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s.%s", conffile.lists_dir, cp1, SUFFIX_OLD);
    break;
  case FILENAME_LIST_BAN :
    len = sprintf(buf, "%s/%s", conffile.lists_dir, "ban");
    break;

  case FILENAME_PROBLEM_d :
    i1 = va_arg(ap,int);
    len = sprintf(buf, "%s/xxqj%d.sgf", conffile.problem_dir, i1);
    break;
  case FILENAME_SPOOL :
    len = sprintf(buf, "%s", conffile.spool_dir);
    break;
  case FILENAME_SPOOL_s :
    cp1 = va_arg(ap,char*);
    len = sprintf(buf, "%s/%s", conffile.spool_dir, cp1);
    break;
  case FILENAME_SPOOL_sd :
    cp1 = va_arg(ap,char*);
    i1 = va_arg(ap,int);
    len = sprintf(buf, "%s/%s%d", conffile.spool_dir, cp1, i1);
    break;
  default: /* This invalid filename will fail on open, and show up in the log ... */
    len = sprintf(buf, "/There/was/a/default/filename:%d", num);
    break;
  }

#if 0 /* Test trap ... */
if (strstr(buf,"home/nngs/nngs/share/nngssrv/stats/player_data/j/joop"))
	raise(5 /* SIGTRAP */ );
#endif

return len;
}

char * filename(void)
{
  return filename1;
}

	/* This function does about the same as "mkdir -p"
	** It assumes *name to be a pathname/filename.
	** all the nodes in the pathname-part are created
	** (if they don't exist yet)
	** duplicate slashes are ignored.
	** NOTE: the part _after_ the last slash is not created;
	**  to indicate an empty directory add a trailing slash 
	** to the argument, (and omit the filename part).
	** So, "/path/to/file.txt" , "/path/to/" and "//path//to/" all should give the same result.
	** Function fails on first unrecoverable) error.
	** returns :
	** (return >= 0) := number of nodes that were created
	** (return < 0) := -errno
	*/
static int mkdir_p(const char * pathname, int mode)
{
  int err = 0;
  size_t len;
  int rc = 0;
  int cnt = 0;
  char *slash;
  char buff[MAX_FILENAME_SIZE];
  struct stat statbuff;

  len = strlen(pathname);
  if (len >= sizeof buff) return -EINVAL;
  memcpy(buff,pathname,len);
  buff[len] = 0;

  for(slash = buff; (slash = strchr(slash+1, '/' )); ) {
	/* this is to catch double / in paths */
    if (slash[-1] == '/') continue;
    *slash = 0 ;
    rc = stat(buff, &statbuff);
    if (!rc) err = EEXIST; /* this is used to skip existing prefix */
    else {
      rc = mkdir(buff, mode);
      err = (rc) ? errno: 0;
      }
    switch(err) {
    case 0:
      cnt++;
      Logit("Mkdir_p[%d](\"%s\", %04o) := Ok", cnt, buff, mode );
    case EEXIST: 
      break;
    case ENOTDIR: 
    case EACCES: 
    default:
      Logit("Mkdir_p(\"%s\", %04o) := [rc=%d] Err= %d (%s)"
           , buff, mode, rc, err, strerror(err) );
      *slash = '/' ;
      goto quit;
    }
    *slash = '/' ;
  }
quit:
  return (err) ? -err : cnt;
}


int pcn_out(int p, int code, int num, ...)
{
  va_list ap;
  int retval;
  const char *format;

  va_start(ap, num);

  format = find_format(parray[p].language,num);
  retval = pcvprintf(p, code, format, ap);

  va_end(ap);
  return retval;
}


int pcn_out_prompt(int p, int code, int num,...)
{
  va_list ap;
  int retval;
  const char *format;

  va_start(ap, num);

  format = find_format(parray[p].language,num);
  retval = pcvprintf(p, code, format, ap);

  pprompt(p);

  va_end(ap);
  return retval;
}

/* AvK This is just a wrapper around crypt.
** some platforms have no working crypt()
** , but do contain a stub, which only returns NULL.
**
** We first call crypt(), if it returns NULL: we issue a warning and bail out.
** Desperate people could return something usefull here.
*/
char * mycrypt(const char *passwd, const char * salt)
{
  char *cp;

  cp = crypt(passwd, salt);
  if (!cp) {
    fprintf(stderr,"\n%s,line %d: Need a working crypt() function!\n"
           , __FILE__,__LINE__);
    Logit("%s,line %d: Need a working crypt() function!"
           , __FILE__,__LINE__);
    main_exit(0);
  }
  return cp;
}

int xystat(struct stat * sp, int num, ...)
{
  va_list ap;
  int rc;
  struct stat local;

  va_start(ap, num);

  memset(filename1,0,sizeof filename1);
  vafilename(filename1,num, ap);
  if (!sp) sp = &local;
  rc = stat(filename1, sp);

  va_end(ap);
  if (!rc) rc = sp->st_size;
  return rc;
}

int xytouch(int num, ...)
{
  va_list ap;
  int rc;

  va_start(ap, num);

  /* memset(filename1,0,sizeof filename1); */
  vafilename(filename1,num, ap);
  rc = utime(filename1, NULL);

  va_end(ap);
  return rc;
}

DIR * xyopendir(int num, ...)
{
  va_list ap;
  DIR * dirp;

  va_start(ap, num);

  vafilename(filename1,num, ap);
  dirp = opendir(filename1);

  va_end(ap);
  return dirp;
}

int parse_rank(int num, int ch)
{

  switch (ch) {
  case 'k': case 'K': num = 31 - num; break;
  case 'd': case 'D': num += 30; break;
  case 'p': case 'P': num += 40; break;
  default: /* Bad rating. */ break;
  }
  return num;
}


int asc2ipaddr(char *str, unsigned *add)
{
  int rc;
  unsigned vals[4];

  str += strspn(str,"0xX");
  rc = sscanf(str, "%u.%u.%u.%u", vals,vals+1,vals+2, vals+3);
  switch(rc) {
  case 0:
  case 1:
    rc = sscanf(str, "%x", vals);
    if (rc != 1) break;
    *add = vals[0];
    return 0;
  default: break;
  case 4:
    *add
      = (vals[0]&0xff) <<24
      |(vals[1]&0xff) <<16
      |(vals[2]&0xff) <<8
      |(vals[3]&0xff)  ;
    return 0;
  }
  return -1;
}
