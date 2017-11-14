#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "rb.h"
#include "update-command-not-found.h"

static int collect_contents(struct rb_root *t_bin,
			    struct rb_root *t_pkg,
			    size_t *pkg_len) {
	_cleanup_fclose_ FILE *contents_cat = NULL;
	char buf[8192], *bin, *pkg, *t, *t2;
	size_t p_len = 0;

	if (!(contents_cat = popen(
"ls /var/lib/apt/lists | grep --exclude=diff_Index Contents-`dpkg --print-architecture` | while read -r line; do\n" \
"	/usr/lib/apt/apt-helper cat-file /var/lib/apt/lists/$line | grep bin/\n"\
"done\n", "r")))
		return -ENOENT;/*this is just a guess...*/
	do {
		struct binary *node;
		struct rb_node *n;

		if (!fgets((char *)&buf, sizeof(buf), contents_cat))
			break;
		if (!memcmp("usr/bin", &buf, strlen("usr/bin")) ||
		    !memcmp("usr/sbin", &buf, strlen("usr/sbin")) ||
		    !memcmp("bin", &buf, strlen("bin")) ||
		    !memcmp("sbin", &buf, strlen("sbin")) ||
		    !memcmp("usr/games", &buf, strlen("usr/games")))
			/*go below*/;
		else
			continue;

		t = strchr((char *)&buf, ' ');
		if (!t)
			return -EINVAL;
		*t = '\0';
		t2 = strrchr((char *)&buf, '/');
		if (!t2)
			t2 = (char *)&buf - 1;
		bin = t2 + 1;

		/*get package name*/
		t = strrchr(t + 1, '/');
		if (!t)
			return -EINVAL;
		pkg = t + 1;
		*(t + strlen(t) - 1) = '\0'; /*kill the newline*/

		//printf("%s %s\n", bin, pkg);
		node = malloc(sizeof(struct binary));
		node->bin = strdup(bin);
		node->pkg = strdup(pkg);
		if (!node->pkg || !node->bin)
			return -ENOMEM;

		rb_insert(&node->n_bin, t_bin);

		p_len += strlen(pkg) + 1;
		p_len += strlen(bin) + 1;
	} while (true);

	if (pkg_len)
		*pkg_len = p_len;

	return 0;
}

static inline int compare_bin(const struct rb_node *a,
			      const struct rb_node *b) {
	struct binary *p = to_binary(a, n_bin);
	struct binary *q = to_binary(b, n_bin);

	return strcmp(p->bin, q->bin);
}

static void construct_az_list(struct rb_root *bin,
				  char           *pkg_list,
				  size_t pkg_list_len) {
	struct binary *b;
	struct rb_node *n;
	char *t;

	t = pkg_list;

	n = rb_first(bin);
	b = rb_entry(n, struct binary, n_bin);
	do {
		strcpy(t, b->bin);
		t += strlen(t) + 1;
		*(t - 1) = '\0';
		strcpy(t, b->pkg);
		t += strlen(t) + 1;
		*(t - 1) = '\n';

		n = rb_next(&b->n_bin);
		if (!n)
			break;
		b = to_binary(n, n_bin);
	} while (true);
}

int main(int argc, char *argv[]) {
	struct rb_root bin, pkg;
	_cleanup_free_ char *bin_list = NULL;
	size_t len;
	int r;
	char db_tmp[] = "/var/cache/command-not-found/db";
	_cleanup_fclose_ FILE *db = NULL;

	rb_init(&bin, compare_bin, 0);

	if ((r = collect_contents(&bin, &pkg, &len)) < 0)
		return r;
	if (!(bin_list = malloc(len))) /* We know the length. */
		return -ENOMEM;
	construct_az_list(&bin, bin_list, len);
	
	db = fopen(db_tmp, "w");
	fwrite(bin_list, 1, len, db);
	return 0;
}
