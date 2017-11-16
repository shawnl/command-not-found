/* command-not-found, finds programs
 * Copyright (C) 2017  Shawn Landden <slandden@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <libintl.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#define _ gettext

#include "command-not-found.h"
#include "pts_lbsearch.h"

bool arg_ignore_installed = false;
char *arg_command = NULL;

static const char *components[] = {"main", "contrib", "non-free", "universe",
		"multiverse", "restricted", NULL};

int can_sudo() {
	struct group *adm, *sudo;
	gid_t mygroups[1024];
	int r;

	adm = getgrnam("adm");
	sudo = getgrnam("sudo");
	if (!adm || !sudo)
		return -errno;
	r = getgroups(sizeof(mygroups) / sizeof(gid_t), mygroups);
	if (r < 0)
		return -errno;

	for (int i = 0; i < r; i++)
		if (mygroups[i] == adm->gr_gid || mygroups[i] == sudo->gr_gid)
			return 0;

	return -ENOENT;
}

bool is_root() {
	return (geteuid() == 0);
}

int get_entry(char *out, size_t out_len, char *command_ff_terminated) {
	int r;

	r = pts_lbsearch_main(4, STRV_MAKE(
		"/usr/share/command-not-found/pts_lbsearch", "-p",
		"/var/cache/command-not-found/db", command_ff_terminated),
		out, out_len);
	if (r < 0)
		return -ENOENT;
	if (strlen(out) == 0)
		return -ENOENT;

	return 0;
}

void spell_check_print_header(char *command) {
	static bool printed_header = false;

	if (printed_header == false) {
		dprintf(2, "No command '%s' found, did you mean:\n", command);
		printed_header = true;
	}
}

/*def similar_words(word):
    """ return a set with spelling1 distance alternative spellings

        based on http://norvig.com/spell-correct.html"""
    alphabet = 'abcdefghijklmnopqrstuvwxyz-_'
    s = [(word[:i], word[i:]) for i in range(len(word) + 1)]
    deletes    = [a + b[1:] for a, b in s if b]
    transposes = [a + b[1] + b[0] + b[2:] for a, b in s if len(b)>1]
    replaces   = [a + c + b[1:] for a, b in s for c in alphabet if b]
    inserts    = [a + c + b     for a, b in s for c in alphabet]
    return set(deletes + transposes + replaces + inserts)*/

/* I don't think thse 4 loops can be combined, as they are subtely differn't.
 * Maybe in ruby with a lambda, but not in C
 */
void spell_check(char *command) {
	char alphabet[] = "abcdefghijklmnopqrstuvwxyz-_";
	char buf[4096], *component, *package, *s;
	char bufout[4096];

	if (strlen(command) < 4 || strlen(command) >= sizeof(buf) - 2)
		return;
	/* deletes */
	for (int i = 0; i < strlen(command); i++) {
		memcpy(buf, command, i);
		memcpy(buf + i, command + i + 1, strlen(command) - (i + 1));
		buf[strlen(command) - 1] = '\xff';
		buf[strlen(command)] = '\0';
		if (get_entry(bufout, sizeof(bufout), buf) != 0)
			continue;
		spell_check_print_header(command);
		package = strchrnul(bufout, '\xff');
		*package = '\0'; package++;
		component = strchrnul(package, '/');
		*component = '\0'; component++;
		*strchrnul(component, '\n') = '\0';
		s = strv_find_prefix((char **)components, component);
		if (!s)
			continue;
		dprintf(2, " Command '%s' from package '%s' (%s)\n", bufout, package, s);
	}
	/* transposes */
	for (int i = 0; i < (strlen(command) - 1); i++) {
		strcpy(buf, command);
		buf[i] = command[i+1];
		buf[i+1]=command[i];

		buf[strlen(command)] = '\xff';
		buf[strlen(command) + 1] = '\0';
		if (get_entry(bufout, sizeof(bufout), buf) != 0)
			continue;
		spell_check_print_header(command);
		package = strchrnul(bufout, '\xff');
		*package = '\0'; package++;
		component = strchrnul(package, '/');
		*component = '\0'; component++;
		*strchrnul(component, '\n') = '\0';
		s = strv_find_prefix((char **)components, component);
		if (!s)
			continue;
		dprintf(2, " Command '%s' from package '%s' (%s)\n", bufout, package, s);
	}
	/* replaces */
	for (int i = 0; i < strlen(command); i++) {
		for (int j = 0; j < strlen(alphabet); j++) {
			memcpy(buf, command, i);
			buf[i] = alphabet[j];
			memcpy(buf + i + 1, command + i + 1, strlen(command) - i - 1);
			buf[strlen(command)] = '\xff';
			buf[strlen(command) + 1] = '\0';
			if (get_entry(bufout, sizeof(bufout), buf) != 0)
				continue;
			spell_check_print_header(command);
			package = strchrnul(bufout, '\xff');
			*package = '\0'; package++;
			component = strchrnul(package, '/');
			*component = '\0'; component++;
			*strchrnul(component, '\n') = '\0';
			s = strv_find_prefix((char **)components, component);
			if (!s)
				continue;
			dprintf(2, " Command '%s' from package '%s' (%s)\n", bufout, package, s);
		}
	}
	/* inserts */
	for (int i = 0; i < strlen(command); i++) {
		for (int j = 0; j < strlen(alphabet); j++) {
			memcpy(buf, command, i);
			buf[i] = alphabet[j];
			memcpy(buf + i + 1, command + i, strlen(command) - i);
			buf[strlen(command) + 1] = '\xff';
			buf[strlen(command) + 2] = '\0';
			if (get_entry(bufout, sizeof(bufout), buf) != 0)
				continue;
			spell_check_print_header(command);
			package = strchrnul(bufout, '\xff');
			*package = '\0'; package++;
			component = strchrnul(package, '/');
			*component = '\0'; component++;
			*strchrnul(component, '\n') = '\0';
			s = strv_find_prefix((char **)components, component);
			if (!s)
				continue;
			dprintf(2, " Command '%s' from package '%s' (%s)\n", bufout, package, s);
		}
	}
}

static void help(void) {
	printf("%s [OPTIONS...] COMMAND ...\n\n"
		"Query commands not available.\n\n"
		"  -h --help                Show this help message\n"
		"     --ignore-installed Do not suggest programs available locally\n\n",
		program_invocation_short_name);
}

static int parse_argv(int argc, char *argv[]) {

	enum {
		ARG_IGNORE_INSTALLED,
	};

	static const struct option options[] = {
		{ "help",                no_argument,       NULL, 'h'                     },
		{ "ignore_installed",    no_argument,       NULL, ARG_IGNORE_INSTALLED    },
		{}
	};

	int c;

	assert(argc >= 0);
	assert(argv);

	while ((c = getopt_long(argc, argv, "hH:M:", options, NULL)) >= 0)

		switch (c) {

		case 'h':
			help();
			return 0;

		case ARG_IGNORE_INSTALLED:
			arg_ignore_installed = true;
			break;

		default:
			assert(false);
		}

	arg_command = argv[argc - 1];
	return 1;
}

int main(int argc, char *argv[]) {
	int r, r2;
	_cleanup_free_ char *v = NULL, *sources_list = NULL;
	size_t sz;
	char buf[8192], buf2[4096], *package, *s, *component;
	char *prefixes[] = {"/usr/bin/", "/usr/sbin/", "/bin/", "/sbin/",
			"/usr/local/bin/", "/usr/games/", NULL};

	r = parse_argv(argc, argv);
	if (r <= 0) {
		help();
		return 1;
	}


	STRV_FOREACH(s, *prefixes) {
		_cleanup_free_ char *w = NULL;
		char *path;

		r = asprintf(&w, "%s%s", s, arg_command);
		if (r < 0)
			goto fail;
		r = access(w, X_OK);
		if (r != 0)
			continue;
		path = getenv("PATH");
		do {
			if (*path == '\0')
				break;
			r = strncmp(path, s, strlen(s));
			if (r != 0)
				continue;
			dprintf(2, _("The command could not be "\
"located because '%s' is not included in the PATH environment variable.\n"), s);
			if (strcmp(s + strlen(s) - 5, "sbin/") == 0)
				dprintf(2, _("This is most likely caused by the"\
" lack of administrative priviledges associated with your user account.\n"));
			return EXIT_SUCCESS;
		} while ((path = strchrnul(path, ':') + 1));

		if (arg_ignore_installed)
			break;

		dprintf(2, _("Command '%s' is available in '%s'\n"), arg_command,
			w);
		return EXIT_SUCCESS;
	}

	r = access("/usr/bin/apt", X_OK);
	r2= access("/usr/bin/aptitude", X_OK);
	if (r != 0 && r2 != 0)
		goto bail;

	r = access("/var/cache/command-not-found/db", R_OK);
	if (r != 0) {
		if (errno == ENOENT)
			dprintf(2, "/var/cache/command-not-found/db not found."
				"run 'update-command-not-found' as root\n");
		goto fail;
	}

	sz = snprintf(buf2, sizeof(buf2), "%s\xff", arg_command);
	if (sz <= 0)
		goto fail;
	r = get_entry(buf, sizeof(buf), buf2);
	if (r < 0) {
		if (r == -ENOENT) {
			spell_check(arg_command);
			goto bail;
		} else
			goto fail;
	}
	package = strchr(buf, '\xff');
	if (!package)
		goto bail;
	package++;
	component = strchr(package, '/');
	if (!component)
		goto bail;
	*component = '\0';
	component++;
	dprintf(2, _("The program '%s' is currently not installed. "), arg_command);
	if (is_root()) {
		dprintf(2, _("You can install it by typing:\n"));
		dprintf(2, _("apt install %s\n"), package);
	} else if (can_sudo() == 0) {
		dprintf(2, _("You can install it by typing:\n"));
		dprintf(2, _("sudo apt install %s\n"), package);
	} else
		dprintf(2, _("To run '%s' please ask your "
			"administrator to install the package '%s'\n"),
			arg_command, package);

	*strchrnul(component, '\n') = '\0';

	s = strv_find_prefix((char **)components, component);
	if (!s)
		goto success;

	// TODO This is hacky, and doesn't support DEB822-style or sources.list.d
	// use libapt-pkg with a C++ to C wrapper <apt-pkg/sourcelist.h>
	r = read_full_file("/etc/apt/sources.list", &sources_list, &sz);
	if (r < 0)
		goto fail;
	s = strstr(sources_list, s);
	if (!s)
		goto component_print;

	s += strcspn(s, "#\n");
	if (!s)
		goto component_print;
	*s = '\0';

	/* not commented out */
	if (strrchr(s, '#') < strrchr(s, '\n'))
component_print:
		dprintf(2, _("You will have to enable the component called '%s'\n"), s);
success:
	return EXIT_SUCCESS;
fail:
	fputs(strerror(errno), stderr);
	fputc('\n', stderr);
	return EXIT_FAILURE;
bail:
	dprintf(2, "%s: command not found\n", arg_command);
	return EXIT_SUCCESS;
}

