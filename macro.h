#pragma once

#include <stdio.h>
#include <assert.h>

#define Cleanup_(x) __attribute__((cleanup(x)))
#define DEFINE_TRIVIAL_CLEANUP_FUNC(type, func)                                 \
        static inline void func##p(type *p) {                                   \
                if (*p)                                                         \
                        func(*p);                                               \
        }                                                                       \
        struct __useless_struct_to_allow_trailing_semicolon__

DEFINE_TRIVIAL_CLEANUP_FUNC(char*, free);
#define Cleanup_free_ Cleanup_(freep)

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

char *strv_find_prefix(char **l, const char *name) {
        char **i;

        assert(name);

        STRV_FOREACH(i, l)
                if (startswith(*i, name))
                        return *i;

        return NULL;
}
