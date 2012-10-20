/* $Id: xdbm.c,v 1.4 2003/06/06 09:51:32 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-04-19.
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
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include "xdbm.h"

#if defined(GDBM) || defined(NDBM)

/* Buggy Linux include files. */
#if !defined(S_IRWXU) || !defined(S_IRGRP) || !defined(S_IROTH)
#define CREATE_MODE 0744
#else
#define CREATE_MODE (S_IRWXU|S_IRGRP|S_IROTH)
#endif

#endif /* defined(GDBM) || defined(NDBM) */


#ifdef GDBM

#define DBM_CREATE(N) \
  gdbm_open((N), 0, GDBM_WRCREAT, CREATE_MODE, 0)
#define DBM_OPEN(N, W) \
  gdbm_open((N), 0, ((W) ? GDBM_WRITER : GDBM_READER), 0, 0)
#define DBM_CLOSE(DB) \
  gdbm_close(DB)
#define DBM_STORE(DB, K, D) \
  gdbm_store((DB), (K), (D), GDBM_REPLACE)
#define DBM_FETCH(DB, K) \
  gdbm_fetch((DB), (K))
#define DBM_DELETE(DB, K) \
  gdbm_delete((DB), (K))
#define DBM_FIRSTKEY(DB) \
  gdbm_firstkey(DB)
#define DBM_NEXTKEY(DB, K) \
  gdbm_nextkey((DB), (K))

#else  /* GDBM */

#ifdef NDBM

#define DBM_CREATE(N) \
  dbm_open(N, O_CREAT|O_RDWR, CREATE_MODE)
#define DBM_OPEN(N, W) \
  dbm_open(N, ((W) ? O_RDWR : O_RDONLY), 0)
#define DBM_CLOSE(DB) \
  dbm_close(DB)
#define DBM_STORE(DB, K, D) \
  dbm_store((DB), (K), (D), DBM_REPLACE)
#define DBM_FETCH(DB, K) \
  dbm_fetch((DB), (K))
#define DBM_DELETE(DB, K) \
  dbm_delete((DB), (K))
#define DBM_FIRSTKEY(DB) \
  dbm_firstkey(DB)
#define DBM_NEXTKEY(DB, K) \
  dbm_nextkey(DB)

#else  /* NDBM */

/*
** Implement our own database, using directories and files
*/

#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

/* On Linux for instance... :-( */
#if !defined(MAXNAMELEN)
#define MAXNAMELEN 256
#endif

struct xdbm
{
  int writing;
  char *name;
};

typedef struct
{
  char *dptr;
  int dsize;
} datum;

static xdbm_t
ddbm_create(char *name)
{
  xdbm_t db = (xdbm_t)malloc(sizeof(struct xdbm));

  if (db)
  {
    db->writing = 1;
    if ((db->name = strdup(name)) == NULL)
    {
      free(db);
      db = NULL;
    }
  }
  return db;
}

static xdbm_t
ddbm_open(char *name, int writing)
{
  struct stat sbuf;

  if (stat(name, &sbuf) == 0)
  {
    if (S_ISDIR(sbuf.st_mode))
    {
      xdbm_t db = ddbm_create(name);
      if (db)
	db->writing = writing;
      return db;	/* Exists and is a dir. */
    }
    else
      return NULL;		/* Exists and is something else. */
  }
  return NULL;			/* Doesn't exist. */
}

static void
ddbm_close(xdbm_t db)
{
  if (db)
  {
    if (db->name)
      free(db->name);
    free(db);
  }
}

static int
ddbm_mkdir(char *dir, int create)
{
  struct stat sbuf;

  if (stat(dir, &sbuf) == 0)
    if (S_ISDIR(sbuf.st_mode))
      return 0;
    else
      return -1;		/* Exists but is not a dir. */
  else
    if (!create)
      return -1;		/* Doesn't exist, and don't create. */
    else
      if (mkdir(dir, 0755) < 0)
	return -1;
      else
	return 0;
}

static int
ddbm_mkpath(xdbm_t db, datum k, char *path, size_t pathmax, int create)
{
  char dir[MAXPATHLEN];
  char name[MAXNAMELEN];

  if (ddbm_mkdir(db->name, create) < 0)
    return -1;

  strncpy(name, k.dptr, k.dsize);
  name[k.dsize] = '\0';

  snprintf(dir, sizeof(dir), "%s/%c", db->name, name[0]);
  if (ddbm_mkdir(dir, create) < 0)
    return -1;

  snprintf(path, pathmax, "%s/%s", dir, name);
  return 0;
}

static int
ddbm_store(xdbm_t db, datum k, datum d)
{
  char path[MAXPATHLEN];
  FILE *fp;

  if (k.dptr == NULL || k.dsize == 0)
    return -1;
  if (ddbm_mkpath(db, k, path, sizeof(path), 1) < 0)
    return -1;
  if (!db->writing)
    return -1;
  if ((fp = fopen(path, "w")) == NULL)
    return -1;
  if (fwrite(d.dptr, 1, d.dsize, fp) != d.dsize)
  {
    fclose(fp);
    unlink(path);
    return -1;
  }
  fclose(fp);
  return 0;
}

static datum
ddbm_fetch(xdbm_t db, datum k)
{
  datum d;
  char path[MAXPATHLEN];
  FILE *fp;
  static void *dptr = NULL;
  static size_t dsize = 32;

  if (k.dptr == NULL || k.dsize == 0)
    goto err1;
  if (ddbm_mkpath(db, k, path, sizeof(path), 0) < 0)
    goto err1;
  if ((fp = fopen(path, "r")) == NULL)
    goto err1;
  if (dptr == NULL)
  {
    if ((dptr = malloc(dsize)) == NULL)
      goto err2;
  }
  {
    size_t x, n = dsize, p = 0;

    while ((x = fread(((char *)dptr)+p, 1, n, fp)) == n)
    {
      p += x;
      dsize += n;
      if ((dptr = realloc(dptr, dsize)) == NULL)
	goto err2;
    }
    p += x;
    fclose(fp);
    if (p == 0)
      goto err1;
    d.dptr = dptr;
    d.dsize = p;

    return d;
  }

err2:
  fclose(fp);
err1:
  d.dptr = NULL;
  d.dsize = 0;
  return d;
}

static int
ddbm_delete(xdbm_t db, datum k)
{
  struct stat sbuf;
  char path[MAXPATHLEN];

  if (k.dptr == NULL || k.dsize == 0)
    return -1;
  if (ddbm_mkpath(db, k, path, sizeof(path), 0) < 0)
    return -1;
  if (!db->writing)
    return -1;
  if (stat(path, &sbuf) < 0)
    return 0;
  if (!S_ISREG(sbuf.st_mode))
    return -1;			/* Not a file. */
  return unlink(path);
}

static DIR *Ddbm_dp1 = NULL, *Ddbm_dp2 = NULL;
static char Ddbm_kptr[MAXNAMELEN] = "";

static datum
ddbm_firstkey(xdbm_t db)
{
  datum k;
  struct dirent *dent1;

  if ((Ddbm_dp1 = opendir(db->name)))
  {
    while ((dent1 = readdir(Ddbm_dp1)))
    {
      char path[MAXPATHLEN];
      struct dirent *dent2;

      if (dent1->d_name[0] == '.')
	continue;
      snprintf(path, sizeof(path), "%s/%s", db->name, dent1->d_name);
      if ((Ddbm_dp2 = opendir(path)))
      {
	while ((dent2 = readdir(Ddbm_dp2)))
	{
	  if (dent2->d_name[0] == '.')
	    continue;
	  strncpy(Ddbm_kptr, dent2->d_name, MAXPATHLEN);
	  Ddbm_kptr[MAXPATHLEN-1] = '\0';
	  k.dptr = Ddbm_kptr;
	  k.dsize = strlen(k.dptr)+1;
	  return k;
	}
	closedir(Ddbm_dp2);
	Ddbm_dp2 = NULL;
      }
    }
    closedir(Ddbm_dp1);
    Ddbm_dp1 = NULL;
  }
  k.dptr = NULL;
  k.dsize = 0;
  return k;
}

static datum
ddbm_nextkey(xdbm_t db)
{
  datum k;
  struct dirent *dent1 = NULL;

  if (Ddbm_dp1 && Ddbm_dp2)
  {
    do {
      struct dirent *dent2;

      if (Ddbm_dp2 == NULL)
      {
	char path[MAXPATHLEN];

	if (dent1 == NULL)
	  continue;
	snprintf(path, sizeof(path), "%s/%s", db->name, dent1->d_name);
	if ((Ddbm_dp2 = opendir(path)) == NULL)
	  continue;
      }
      while ((dent2 = readdir(Ddbm_dp2)))
      {
	if (dent2->d_name[0] == '.')
	  continue;
	strncpy(Ddbm_kptr, dent2->d_name, MAXPATHLEN);
	Ddbm_kptr[MAXPATHLEN-1] = '\0';
	k.dptr = Ddbm_kptr;
	k.dsize = strlen(k.dptr)+1;
	return k;
      }
      closedir(Ddbm_dp2);
      Ddbm_dp2 = NULL;

    } while ((dent1 = readdir(Ddbm_dp1)));

    closedir(Ddbm_dp1);
    Ddbm_dp1 = NULL;
  }

  k.dptr = NULL;
  k.dsize = 0;
  return k;
}

#define DBM_CREATE(N) \
  ddbm_create(N)
#define DBM_OPEN(N,W) \
  ddbm_open(N,W)
#define DBM_CLOSE(DB) \
  ddbm_close(DB)
#define DBM_STORE(DB, K, D) \
  ddbm_store((DB), (K), (D))
#define DBM_FETCH(DB, K) \
  ddbm_fetch((DB), (K))
#define DBM_DELETE(DB, K) \
  ddbm_delete((DB), (K))
#define DBM_FIRSTKEY(DB) \
  ddbm_firstkey(DB)
#define DBM_NEXTKEY(DB, K) \
  ddbm_nextkey(DB)

#endif /* NDBM */
#endif /* GDBM */


xdbm_t
xdbm_create(char *dbname)
{
  xdbm_t db;

  assert(dbname != NULL);
  db = DBM_CREATE(dbname);
  return db;
}

xdbm_t
xdbm_open(char *dbname, int writing)
{
  xdbm_t db;

  assert(dbname != NULL);
  db = DBM_OPEN(dbname, writing);
  return db;
}

void
xdbm_close(xdbm_t db)
{
  assert(db != NULL);
  DBM_CLOSE(db);
}

int
xdbm_store(xdbm_t db, char *key, void *dptr, size_t dsize)
{
  datum k, d;
  char n[XDBM_KEYMAXLEN];
  unsigned i;

  assert(db != NULL && key != NULL && dptr != NULL);
  for (i = 0 ; i < XDBM_KEYMAXLEN-1 && key[i] != '\0' ; i++)
    n[i] = toupper(key[i]);
  n[i++] = '\0';
  k.dptr = n;
  k.dsize = i;
  d.dptr = (char *)dptr;
  d.dsize = dsize;
  return (DBM_STORE(db, k, d) < 0 ? 0 : 1);
}

int
xdbm_fetch(xdbm_t db, char *key, void *dptr, size_t dsize)
{
  size_t s;
  datum k, d;
  char n[XDBM_KEYMAXLEN];
  unsigned i;

  assert(db != NULL && key != NULL && dptr != NULL);
  for (i = 0 ; i < XDBM_KEYMAXLEN-1 && key[i] != '\0' ; i++)
    n[i] = toupper(key[i]);
  n[i++] = '\0';
  k.dptr = n;
  k.dsize = i;
  d = DBM_FETCH(db, k);
  if (d.dptr == NULL)
    return 0;
  s = (d.dsize < dsize ? d.dsize : dsize);
  while (s--)
    ((char *)dptr)[s] = d.dptr[s];
  return 1;
}

int
xdbm_delete(xdbm_t db, char *key)
{
  datum k;
  char n[XDBM_KEYMAXLEN];
  unsigned i;

  assert(db != NULL && key != NULL);
  for (i = 0 ; i < XDBM_KEYMAXLEN-1 && key[i] != '\0' ; i++)
    n[i] = toupper(key[i]);
  n[i++] = '\0';
  k.dptr = n;
  k.dsize = i;
  return (DBM_DELETE(db, k) < 0 ? 0 : 1);
}

static datum Nextkey;

void
xdbm_start(xdbm_t db)
{
  assert(db != NULL);
  Nextkey = DBM_FIRSTKEY(db);
}

int
xdbm_next(xdbm_t db, void *dptr, size_t dsize)
{
  size_t s;
  datum d;

  assert(db != NULL && dptr != NULL);
  if (Nextkey.dptr == NULL)
    return 0;
  d = DBM_FETCH(db, Nextkey);
  if (d.dptr == NULL)
    return 0;
  s = (d.dsize < dsize ? d.dsize : dsize);
  while (s--)
    ((char *)dptr)[s] = d.dptr[s];
  Nextkey = DBM_NEXTKEY(db, Nextkey);
  return 1;
}
