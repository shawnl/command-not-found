#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

#define READ_FULL_BYTES_MAX (4U*1024U*1024U)
#define LINE_MAX (48*1024)

int read_full_stream(FILE *f, char **contents, size_t *size) {
        size_t n, l;
        _cleanup_free_ char *buf = NULL;
        struct stat st;

        assert(f);
        assert(contents);

        if (fstat(fileno(f), &st) < 0)
                return -errno;

        n = LINE_MAX;

        if (S_ISREG(st.st_mode)) {

                /* Safety check */
                if (st.st_size > READ_FULL_BYTES_MAX)
                        return -E2BIG;

                /* Start with the right file size, but be prepared for files from /proc which generally report a file
                 * size of 0. Note that we increase the size to read here by one, so that the first read attempt
                 * already makes us notice the EOF. */
                if (st.st_size > 0)
                        n = st.st_size + 1;
        }

        l = 0;
        for (;;) {
                char *t;
                size_t k;

                t = realloc(buf, n + 1);
                if (!t)
                        return -ENOMEM;

                buf = t;
                errno = 0;
                k = fread(buf + l, 1, n - l, f);
                if (k > 0)
                        l += k;

                if (ferror(f))
                        return errno > 0 ? -errno : -EIO;

                if (feof(f))
                        break;

                /* We aren't expecting fread() to return a short read outside
                 * of (error && eof), assert buffer is full and enlarge buffer.
                 */
                assert(l == n);

                /* Safety check */
                if (n >= READ_FULL_BYTES_MAX)
                        return -E2BIG;

                n = ((n * 2) > READ_FULL_BYTES_MAX) ? READ_FULL_BYTES_MAX : (n * 2);
        }

        buf[l] = 0;
        *contents = buf;
        buf = NULL; /* do not free */

        if (size)
                *size = l;

        return 0;
}

int read_full_file(const char *fn, char **contents, size_t *size) {
        _cleanup_fclose_ FILE *f = NULL;

        assert(fn);
        assert(contents);

        f = fopen(fn, "re");
        if (!f)
                return -errno;

        return read_full_stream(f, contents, size);
}
