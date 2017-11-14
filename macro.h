#pragma once

#include <stdio.h>

#define _cleanup_(x) __attribute__((cleanup(x)))
#define DEFINE_TRIVIAL_CLEANUP_FUNC(type, func)                                 \
        static inline void func##p(type *p) {                                   \
                if (*p)                                                         \
                        func(*p);                                               \
        }                                                                       \
        struct __useless_struct_to_allow_trailing_semicolon__

DEFINE_TRIVIAL_CLEANUP_FUNC(char*, free);
DEFINE_TRIVIAL_CLEANUP_FUNC(FILE*, fclose);
#define _cleanup_free_ _cleanup_(freep)
#define _cleanup_fclose_ _cleanup_(fclosep)

#define STRV_FOREACH(s, l)                      \
        for ((s) = (l); (s) && *(s); (s)++)
