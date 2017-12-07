/* Copyright (C) 2017  Shawn Landden <slandden@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 * Inspired by https://github.com/pts/pts-line-bisect
 * by Péter Szabó <pts@fazekas.hu>
 */
#define _GNU_SOURCE
#include <string.h>
#include <sys/types.h>

/* Returns the file offset of the line starting at ofs, or if no line
 * starts their, then the the offset of the next line.
 */
static off_t get_fofs(char *file, size_t size, off_t ofs) {
	char *c;
	if (ofs == 0) return 0;
	if (ofs > size) return size;
	--ofs;
	c = memchr(file + ofs, '\n', size - ofs);
	if (!c) {
		c = memrchr(file + ofs, '\n', ofs);
		if (!c)
			c = file - 1;
	}
	return c - file + 1;
}

/* Compares x[:xsize] with a line read from yf. */
static int compare_line(char *file, size_t size, off_t fofs,
                          const char *x, size_t xsize) {
	if (fofs == size) return 1;  /* Special casing of EOF at BOL. */
	int len = xsize < (size - fofs) ? xsize : (size - fofs);
	/*int i = strcspn(file + fofs, "\n");
	char *t = strndupa(file + fofs, i);
	dprintf(2, "%u %s\n", len, t);*/
	int r = memcmp(file + fofs, x, len);
	return r;
}

static char *bisect_way(
	char *file, size_t size,
	off_t lo, off_t hi,
	const char *x, size_t xsize) {
	off_t mid, midf;
	int cmp_result;
	if (hi + 0ULL > size + 0ULL)
		hi = size;  /* Also applies to hi == -1. */
	if (xsize == 0) {  /* Shortcuts. */
		if (hi == size) return file + hi;
	}
	if (lo >= hi) return file + get_fofs(file, size, lo);
	do {
		mid = (lo + hi) >> 1;
		midf = get_fofs(file, size, mid);
		cmp_result = compare_line(file, size, midf, x, xsize);
		if (cmp_result > 0) {
			hi = mid;
		} else if (cmp_result < 0) {
			lo = mid + 1;
		} else {
			return file + midf;
		}
	} while (lo < hi);
	return NULL;
}

char *bisect_search(char *file, size_t file_size, char *search) {
	return bisect_way(file, file_size, 0, (off_t)-1, search, strlen(search));
}
/*
int main(int argc, char *argv[]) {
	char *file;
	size_t size;
	int fd = -1;
	struct stat st;
	int r;
	char *search = "\377", *result;

	fd = open("./db", O_RDONLY);
	if (fd < 0)
		abort();
	r = fstat(fd, &st);
	if (r < 0)
		abort();
	size = st.st_size;
	file = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	
	result = bisect_way(file, size, 0, (off_t)-1, search, strlen(search));
	//mummap(file, size); no need when the process will close so soon 
	if (!result)
		return EXIT_FAILURE;
	int i = strcspn(result, "\n");
	char *t = strndupa(result, i);
	dprintf(1, "%s\n", t);
	return EXIT_SUCCESS;
}*/
