/*
 * Copyright (C) 2008 Sun Microsystems
 *
 * This file is part of uperf.
 *
 * uperf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * uperf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uperf.  If not, see http://www.gnu.org/licenses/.
 */


#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <strings.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif /* HAVE_ALLOCA_H */
#ifdef HAVE_SYS_SENDFILE_H
#include <sys/sendfile.h>
#endif /* HAVE_SYS_SENDFILE_H */
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif /* HAVE_SYS_UIO_H */

#include "uperf.h"

typedef struct file_list {
	off_t size;
	int fd;
	struct file_list *next;
}file_list_t;

typedef struct sendfilev_list {
	int nfiles;
	char dir[PATHMAX];
	struct file_list *flist;
	struct sendfilev_list *next;
}sfv_list_t;

static sfv_list_t *sfv_list;
static file_list_t fhead;

static int
add_file(const char *name, const struct stat *s, int flag)
{
	int fd;
	file_list_t *fl;
	if ((flag != FTW_F) || (!S_ISREG(s->st_mode)))
		return (0);
	if ((fd = open(name, O_RDONLY)) < 0)
		return (0);
	/* printf("found %s\n", name); */
	if (!(fl = calloc(1, sizeof (file_list_t)))) {
			perror("calloc");
			return (-1);
	}
	fl->size = s->st_size;
	fl->fd = fd;
	fl->next = fhead.next;
	fhead.next  = fl;

	return (0);
}

static sfv_list_t *
find_sfv_list(const char *dir)
{
	sfv_list_t *s;

	for (s = sfv_list; s; s = s->next) {
		if (strncmp(s->dir, dir, PATHMAX) == 0)
			break;
	}
	return (s);
}

/* pick a random file from the list and return its index */
static int
select_file(sfv_list_t *s)
{
	int fileno = rand() % s->nfiles;

/* printf("picked %s\n", s->flist[fileno].name); */
	return (fileno);
}


/* Open all readable files in the dir 'dir' and get their sizes */
int
sendfile_init(char *dir)
{

	int count = 0;
	struct file_list *flist;
	struct file_list *fl;
	sfv_list_t *s;

	if ((s = find_sfv_list(dir)) != NULL)
		return (0);

	bzero(&fhead, sizeof (fhead));

	if (ftw(dir, add_file, 1000) != 0) {
		perror("find(ftw):");
		return (-1);
	}
	/* Now walk the file_list_t list and count entries */
	flist = fhead.next;
	for (fl = fhead.next; fl; fl = fl->next)
		count++;
	assert(count);

	if ((s = calloc(1, sizeof (sfv_list_t))) == NULL) {
		perror("calloc");
		return (-1);
	}
	if ((s->flist = calloc(count, sizeof (struct file_list))) == NULL) {
		perror("calloc");
		return (-1);
	}
	s->nfiles = count;
	flist = s->flist;
	(void) strlcpy(s->dir, dir, sizeof (s->dir));
	for (fl = fhead.next; fl; fl = fl->next) {
		(void) memcpy(flist, fl, sizeof (file_list_t));
		flist++;
	}
	if (sfv_list) {
		s->next = sfv_list->next;
		sfv_list->next = s;
	} else {
		sfv_list = s;
	}

	return (0);
}

#ifdef HAVE_SENDFILEV	/* Linux does not have sendfilev */
static ssize_t
do_sendfilev_chunked(sfv_list_t *s, int sock, int csize)
{
	int size, r, n, xferred;
	struct sendfilevec vec;

	r = select_file(s);
	size = 0;
	vec.sfv_fd = s->flist[r].fd;
	vec.sfv_flag = 0;
	while (size < s->flist[r].size) {
		vec.sfv_off = size;
		vec.sfv_len = MIN(csize, s->flist[r].size - size);

		if ((n = sendfilev(sock, &vec, 1, (size_t *)&xferred)) <= 0)
			return (n);
		size += n;
	}
	return (size);
}

ssize_t
do_sendfilev(int sock, char *dir, int nfiles, int chunk_size)
{
	int i;
	size_t xferred;
	sfv_list_t *s;
	struct sendfilevec *vec, *v;

	s = find_sfv_list(dir);
	assert(s);

	if (chunk_size != 0)
		return (do_sendfilev_chunked(s, sock, chunk_size));

	if ((vec = alloca(nfiles * sizeof (struct sendfilevec))) == 0) {
		perror("calloc");
		return (-1);
	}
	v = vec;
	bzero(vec, nfiles * sizeof (struct sendfilevec));
	for (i = 0; i < nfiles; i++) {
		int r = select_file(s);
		v->sfv_fd = s->flist[r].fd;
		v->sfv_len = s->flist[r].size;
		v++;
	}

	return (sendfilev(sock, vec, nfiles, &xferred));
}
#endif

ssize_t
do_sendfile(int sock, char *dir, int chunk_size)
{
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
	off_t len;
#endif
	off_t off = 0;
	sfv_list_t *s;

	s = find_sfv_list(dir);
	assert(s);

	int r = select_file(s);
	if (chunk_size == 0) {
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
#if defined(UPERF_FREEBSD)
		len = 0;
		if (sendfile(s->flist[r].fd, sock, off, s->flist[r].size, NULL, &len, 0) < 0) {
#else
		len = s->flist[r].size;
		if (sendfile(s->flist[r].fd, sock, off, &len, NULL, 0) < 0) {
#endif
			return (-1);
		} else {
			return (len);
		}
#else
		return (sendfile(sock, s->flist[r].fd, &off, s->flist[r].size));
#endif
	} else {
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
		while (off < s->flist[r].size) {
#if defined(UPERF_FREEBSD)
			len = 0;
			if (sendfile(s->flist[r].fd, sock, off, chunk_size, NULL, &len, 0) < 0) {
#else
			len = chunk_size;
			if (sendfile(s->flist[r].fd, sock, off, &len, NULL, 0) < 0) {
#endif
				return (-1);
			}
			off += len;
		}
#else
		while (off < s->flist[r].size) {
			if (sendfile(sock, s->flist[r].fd, &off, chunk_size) < 0) {
				return (-1);
			}
		}
#endif
		return (s->flist[r].size);
	}
}

#ifdef MAIN
int
main(int argc, char *argv[])
{
	int i;
	sfv_list_t *s;
	srand(gethrtime());
	sendfilev_init(argv[1]);
	sendfilev_init(argv[1]);

	s = find_sfv_list(argv[1]);
	assert(s);
	for (i = 0; i < 10; i++)
		select_file(s);
	return (0);

}
#endif
