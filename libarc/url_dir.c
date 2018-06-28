/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "url.h"

#ifdef __W32READDIR__
#include "readdir.h"
# define NAMLEN(dirent) strlen((dirent)->d_name)
#elif __MACOS__
# include "mac_readdir.h"
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#endif


typedef struct _URL_dir
{
    char common[sizeof(struct _URL)];
    DIR *dirp;
    struct dirent *d;

    char *ptr;
    int len;
    long total;
    char *dirname;
    int endp;
} URL_dir;

static int name_dir_check(char *url_string);
static long url_dir_read(URL url, void *buff, long n);
static char *url_dir_gets(URL url, char *buff, int n);
static long url_dir_tell(URL url);
static void url_dir_close(URL url);

struct URL_module URL_module_dir =
{
    URL_dir_t,			/* type */
    name_dir_check,		/* URL checker */
    NULL,			/* initializer */
    url_dir_open,		/* open */
    NULL			/* must be NULL */
};

static int name_dir_check(char *url_string)
{
    if(strncasecmp(url_string, "dir:", 4) == 0)
	return 1;
    url_string = pathsep_strrchr(url_string);
    return url_string != NULL && *(url_string + 1) == '\0';
}

URL url_dir_open(char *dname)
{
    URL_dir *url;
    DIR *dirp;
    int dlen;

    if(dname == NULL)
	dname = ".";
    else
    {
	if(strncasecmp(dname, "dir:", 4) == 0)
	    dname += 4;
	if(*dname == '\0')
	    dname = ".";
	else
	    dname = url_expand_home_dir(dname);
    }
    dname = safe_strdup(dname);

    /* Remove tail of path sep. */
    dlen = strlen(dname);
    while(dlen > 0 && IS_PATH_SEP(dname[dlen - 1]))
	dlen--;
    dname[dlen] = '\0';
    if(dlen == 0)
	strcpy(dname, PATH_STRING); /* root */

    if((dirp = opendir(dname)) == NULL)
    {
	url_errno = errno;
	free(dname);
	errno = url_errno;
	return NULL;
    }

    url = (URL_dir *)alloc_url(sizeof(URL_dir));
    if(url == NULL)
    {
	url_errno = errno;
	closedir(dirp);
	free(dname);
	errno = url_errno;
	return NULL;
    }

    /* common members */
    URLm(url, type)      = URL_dir_t;
    URLm(url, url_read)  = url_dir_read;
    URLm(url, url_gets)  = url_dir_gets;
    URLm(url, url_fgetc) = NULL;
    URLm(url, url_seek)  = NULL;
    URLm(url, url_tell)  = url_dir_tell;
    URLm(url, url_close) = url_dir_close;

    /* private members */
    url->dirp = dirp;
    url->d = NULL;
    url->ptr = NULL;
    url->len = 0;
    url->total = 0;
    url->dirname = dname;
    url->endp = 0;

    return (URL)url;
}

static long url_dir_tell(URL url)
{
    return ((URL_dir *)url)->total;
}

char *url_dir_name(URL url)
{
    if(url->type != URL_dir_t)
	return NULL;
    return ((URL_dir *)url)->dirname;
}

static void url_dir_close(URL url)
{
    URL_dir *urlp = (URL_dir *)url;
    closedir(urlp->dirp);
    free(urlp->dirname);
    free(urlp);
}

static long url_dir_read(URL url, void *buff, long n)
{
    char *p;

    p = url_dir_gets(url, (char *)buff, (int)n);
    if(p == NULL)
	return 0;
    return (long)strlen(p);
}

static char *url_dir_gets(URL url, char *buff, int n)
{
    URL_dir *urlp = (URL_dir *)url;
    int i;

    if(urlp->endp)
	return NULL;
    if(n <= 0)
	return buff;
    if(n == 1)
    {
	*buff = '\0';
	return buff;
    }
    n--; /* for '\0' */;
    for(;;)
    {
	if(urlp->len > 0)
	{
	    i = urlp->len;
	    if(i > n)
		i = n;
	    memcpy(buff, urlp->ptr, i);
	    buff[i] = '\0';
	    urlp->len -= i;
	    urlp->ptr += i;
	    urlp->total += i;
	    return buff;
	}

	do
	    if((urlp->d = readdir(urlp->dirp)) == NULL)
	    {
		urlp->endp = 1;
		return NULL;
	    }
        while (
#ifdef INODE_AVAILABLE
	       urlp->d->d_ino == 0 ||
#endif /* INODE_AVAILABLE */
               NAMLEN(urlp->d) == 0);
	urlp->ptr = urlp->d->d_name;
	urlp->len = NAMLEN(urlp->d);
    }
}
