/* Copyright (C) 2017  Shawn Landden <shawn@git.icu>
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
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define _ gettext

#include "rb.h"
#include "update-command-not-found.h"
#include "util.h"

static int collect_contents(struct rb_root *t_bin) {
	Cleanup(fclosep) FILE *contents_cat = NULL, *apt = NULL;
	char buf[8192], *bin, *pkg, *cmn, *t, *t2, *t3;
	Cleanup(freep) char *indextargets = NULL, *contents = NULL,
		*popens = NULL;
	int r;

	if (!(apt = popen("apt-get indextargets", "r")))
		return -ENOENT;

	r = read_full_stream(apt, &indextargets, NULL);
	if (r < 0)
		return r;

	t = indextargets;
	contents = realloc(contents, 1);
	*contents = '\0';
	while ((t = strstr(t, "\n\n"))) {
		t += 2;
		t = strchrnul(t, '\n'); t++;/*MetaKey:*/
		if (strncmp(t, "ShortDesc: Contents-", strlen("ShortDesc: Contents-")) != 0)
			continue;
		t = strchrnul(t, '\n'); t++;/*ShortDesc::*/
		t = strchrnul(t, '\n'); t++;/*Description:*/
		t = strchrnul(t, '\n'); t++;/*URI:*/
		if (strncmp(t, "Filename: ", strlen("Filename: ")) != 0)
			continue;
		t += strlen("Filename: ");
		t2 = strchrnul(t, '\n');
		*t2 = '\0';
		contents = realloc(contents, strlen(contents ? contents : "") + strlen(t) + 1 + 1);
		strcat(contents, " ");
		strcat(contents, t);
		*t2 = '\n';
	};

	r = access("/usr/lib/apt/apt-helper", X_OK);
	if (r < 0)
		return -errno;
	if (strlen(contents) == 0) {
		puts(_("Contents of packages not available. Install 'apt-file' and then run 'apt-get update'.\n"));
		return EXIT_FAILURE;
	}
	r = asprintf(&popens, "/usr/lib/apt/apt-helper cat-file %s", contents);
	if (r < 0)
		return -errno;

	if (!(contents_cat = popen(popens,
		"r")))
		return -ENOENT;
	do {
		struct binary *node;
		bool got_component = false;

		if (!fgets((char *)&buf, sizeof(buf), contents_cat))
			break;
		if (!memcmp("usr/bin/", &buf, strlen("usr/bin/")) ||
		    !memcmp("usr/sbin/", &buf, strlen("usr/sbin/")) ||
		    !memcmp("bin/", &buf, strlen("bin/")) ||
		    !memcmp("sbin/", &buf, strlen("sbin/")) ||
		    !memcmp("usr/games/", &buf, strlen("usr/games/")))
			/*go below*/;
		else
			continue;

		t = &buf[strcspn((char *)&buf, " \t")];
		if (!t)
			return -EINVAL;
		*t++ = '\0';
		t2 = strrchr((char *)&buf, '/');
		if (!t2)
			t2 = (char *)&buf - 1;
		bin = t2 + 1;

		/*get package name*/
		t3 = strrchr(t + 1, '/');
		if (!t3)
			return -EINVAL;
		pkg = t3 + 1;
		*(t3 + strlen(t3) - 1) = '\0'; /*kill the newline*/

		/* get component (if it doesn't exist, package is in main) */
		if ((t2 = strchr(t + 1, ','))) {
			t = t2 + 1;
			t2 = strchr(t2 + 1, '/');
		} else
			t2 = strchr(t + 1, '/');
		if (t2 && strchr(t2 + 1, '/')) {
			t2[0] = '\0';
			cmn = t + strspn(t, " \t");
			got_component = true;
		}

		//printf("%s %s\n", bin, pkg);
		node = malloc(sizeof(struct binary));
		if (!node)
			return -ENOMEM;
#ifndef NDEBUG
		node->n_bin.sanity = 4138;
#endif
		node->bin = strdup(bin);
		node->pkg = strdup(pkg);
		if (!node->bin | !node->pkg)
			return -ENOMEM;

		if (got_component) {
			node->cmn = strdup(cmn);
			if (!node->bin)
				return -ENOMEM;
		} else
			node->cmn = "main";

		node->entry = malloc(strlen(bin) + 2);
		if (!node->entry)
			return -ENOMEM;

		strcpy(node->entry, node->bin);
		*strchrnul(node->entry, '\0') = '\xff';
		*(strchrnul(node->entry, '\xff') + 1) = '\0';

		rb_insert(&node->n_bin, t_bin);
	} while (true);

	return 0;
}

static inline int compare_bin(const struct rb_node *a,
			      const struct rb_node *b) {
	struct binary *p = to_binary(a, n_bin);
	struct binary *q = to_binary(b, n_bin);

	return strcmp(p->entry, q->entry);
}

/* These are for debugging. */
static struct binary *get_bin(struct rb_node *node) {return to_binary(node, n_bin);}

int main(int argc, char *argv[]) {
	struct rb_root bin;
	struct binary *b;
	struct rb_node *n;
	int r;
	Cleanup(fclosep) FILE *db = NULL;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	rb_init(&bin, compare_bin, 0);

	if ((r = collect_contents(&bin)) < 0)
		goto fail;

	db = fopen("/var/cache/command-not-found/db", "w+");
	if (!db) {
		r = -errno;
		goto fail;
	}

	n = rb_first(&bin);
	if (!n) {
		return EXIT_FAILURE;
	}
	b = rb_entry(n, struct binary, n_bin);
	do {
		if (strcmp(b->cmn, "main") == 0)
			fprintf(db, "%s\xff%s\n", b->bin, b->pkg);
		else
			fprintf(db, "%s\xff%s/%c\n", b->bin, b->pkg, b->cmn[0]);

		n = rb_next(&b->n_bin);
		if (!n)
			break;
		b = to_binary(n, n_bin);
	} while (true);

	return EXIT_SUCCESS;
fail:
	dprintf(2, "%s", strerror(-r));
	dprintf(2, "\n");
	return EXIT_FAILURE;
}
