#pragma once

#include <stdio.h>
#include <assert.h>
#include <string.h>

#define PACKAGE "command-not-found"
#define LOCALEDIR "/usr/share/locale"

#define ___cleanup_(x) __attribute__((cleanup(x)))
#define DEFINE_TRIVIAL_CLEANUP_FUNC(type, func)                                 \
        static inline void func##p(type *p) {                                   \
                if (*p)                                                         \
                        func(*p);                                               \
        }                                                                       \
        struct __useless_struct_to_allow_trailing_semicolon__

DEFINE_TRIVIAL_CLEANUP_FUNC(char*, free);
DEFINE_TRIVIAL_CLEANUP_FUNC(FILE*, fclose);
#define ___cleanup_free_ ___cleanup_(freep)
#define ___cleanup_fclose_ ___cleanup_(fclosep)

#define STRV_MAKE(...) ((char**) ((const char*[]) { __VA_ARGS__, NULL }))

#define STRV_FOREACH(s, l)                      \
        for ((s) = (l); (s) && *(s); (s)++)

static inline char *startswith(const char *s, const char *prefix) {
        size_t l;

        l = strlen(prefix);
        if (strncmp(s, prefix, l) == 0)
                return (char*) s + l;

        return NULL;
}

static char *strv_find_prefix(char **l, const char *name) {
        char **i;

        assert(name);

        STRV_FOREACH(i, l)
                if (startswith(*i, name))
                        return *i;

        return NULL;
}

int read_full_file(const char *fn, char **contents, size_t *size);
int read_full_stream(FILE *f, char **contents, size_t *size);
