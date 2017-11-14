#pragma once

#include <stdint.h>

#include "macro.h"
#include "rb.h"

struct binary {
	char *bin;
	char *pkg;
	struct rb_node n_bin;
};

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#define	to_binary(ptr, member)	container_of(ptr, struct binary, member)
