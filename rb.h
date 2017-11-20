/*
 * libtree.h - this file is part of Libtree.
 *
 * Copyright (C) 2010-2014 Franck Bui-Huu <fbuihuu@gmail.com>
 *
 * This file is part of libtree which is free software; you can
 * redistribute it and/or modify it under the terms of the GNU Lesser
 * General Public License as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the LICENSE file for license rights and limitations.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * The definition has been stolen from the Linux kernel.
 */

#define rb_entry(node, type, member) ({			\
	const struct rb_node *__mptr = (node);			\
(struct binary *)( (char *)__mptr - offsetof(type, member) );})

/*
 * Red-black tree
 */
enum rb_color {
	RB_BLACK,
	RB_RED,
};

struct rb_node {
	struct rb_node *left, *right;
	uintptr_t parent;
#ifndef NDEBUG
	/* get this to a constant so valid structs can be recognized in
	 * the debugger
	 */
	uint32_t sanity;
#endif
};

typedef int (*rb_cmp_fn_t)(const struct rb_node *, const struct rb_node *);

struct rb_root {
	struct rb_node *root;
	rb_cmp_fn_t cmp_fn;
	uint64_t reserved[4];
};

struct rb_node *rb_first(const struct rb_root *tree);
struct rb_node *rb_last(const struct rb_root *tree);
struct rb_node *rb_next(const struct rb_node *node);
struct rb_node *rb_prev(const struct rb_node *node);

struct rb_node *rb_lookup(const struct rb_node *key, const struct rb_root *tree);
struct rb_node *rb_insert(struct rb_node *node, struct rb_root *tree);
void rb_remove(struct rb_node *node, struct rb_root *tree);
void rb_replace(struct rb_node *old, struct rb_node *node, struct rb_root *tree);
int rb_init(struct rb_root *tree, rb_cmp_fn_t cmp, unsigned long flags);
