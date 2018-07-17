/* command-not-found, finds programs
 * Copyright (C) 2017  Shawn Landden <slandden@gmail.com>
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
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define _ gettext

#include "command-not-found.h"
#include "bisect.h"

bool arg_ignore_installed = false;
char *arg_command = NULL;

char *file;
size_t file_size;

#ifdef CONFIG_OPENWRT
				/* packages is implied */
static const char *components[] = {"base", "luci", "routing",
		"telephony", NULL};
#else /*Debian/Ubuntu */
				/* main is implied */
static const char *components[] = {"contrib", "non-free", "universe",
		"multiverse", "restricted", NULL};
#endif

static int can_sudo() {
	struct group *grp;
	gid_t adm = 0, sudo = 0, wheel = 0;
	gid_t mygroups[8092];
	int r;

	grp = getgrnam("adm");
	if (grp)
		adm = grp->gr_gid;
	grp = getgrnam("sudo");
	if (grp)
		sudo = grp->gr_gid;
	grp = getgrnam("wheel");
	if (grp)
		wheel = grp->gr_gid;
	r = getgroups(sizeof(mygroups) / sizeof(gid_t), mygroups);
	if (r < 0)
		return -errno;

	for (int i = 0; i < r; i++) {
		if (	(mygroups[i] == adm) ||
			(mygroups[i] == sudo) ||
			(mygroups[i] == wheel))
			return 0;
	}

	return -ENOENT;
}

static bool is_root() {
#ifdef CONFIG_OPENWRT
	return true;
#else
	return (geteuid() == 0);
#endif
}

static void spell_check_print_suggestion(const char *command, char *bin, char *package, char *s) {
	static bool printed_header = false;

	if (printed_header == false) {
		fprintf(stderr, _("No command '%s' found, did you mean:"), command);
		fputc('\n', stderr);
		printed_header = true;
	}

	fprintf(stderr, _(" Command '%s' from package '%s' (%s)"), bin, package, s);
	fputc('\n', stderr);
}

static void spell_check_suggestion_search(const char *command, char *buf, unsigned buf_len) {
	char *component, *bin, *package, *s;

	s = bisect_search(file, file_size, buf, buf_len);
	if (!s)
		return;
	bin = strndupa(s, strchrnul(s, '\n') - s + 1);
	package = strchrnul(bin, '\xff');
	*package = '\0'; package++;
	component = strchrnul(package, '/');
	if (*component == '/') {
		*strchrnul(component, '\n') = '\0';
		*component++ = '\0';
		s = strv_find_prefix((char **)components, component);
		if (!s)
			return;
	} else {
		*(component - 1) = '\0';
		s = "main";
	}
	spell_check_print_suggestion(command, bin, package, s);
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
static void spell_check(const char *command) {
	char alphabet[] = "abcdefghijklmnopqrstuvwxyz-_";
	char buf[strlen(command) + 2];

	if (strlen(command) < 4)
		return;
	/* deletes */
	for (int i = 0; i < strlen(command); i++) {
		if (command[i] == command[i + 1])
			continue;
		memcpy(buf, command, i);
		memcpy(buf + i, command + i + 1, strlen(command) - (i + 1));
		buf[strlen(command) - 1] = '\xff';
		buf[strlen(command)] = '\0';
		spell_check_suggestion_search(command, buf, strlen(command));
	}
	/* transposes */
	for (int i = 0; i < (strlen(command) - 1); i++) {
		strcpy(buf, command);
		buf[i] = command[i+1];
		buf[i+1]=command[i];

		buf[strlen(command)] = '\xff';
		buf[strlen(command) + 1] = '\0';
		spell_check_suggestion_search(command, buf, strlen(command));
	}
	/* replaces */
	for (int i = 0; i < strlen(command); i++) {
		for (int j = 0; j < strlen(alphabet); j++) {
			memcpy(buf, command, i);
			buf[i] = alphabet[j];
			memcpy(buf + i + 1, command + i + 1, strlen(command) - i - 1);
			buf[strlen(command)] = '\xff';
			buf[strlen(command) + 1] = '\0';
			spell_check_suggestion_search(command, buf, strlen(command) + 1);
		}
	}
	/* inserts */
	for (int i = 0; i <= strlen(command); i++) {
		for (int j = 0; j < strlen(alphabet); j++) {
			memcpy(buf, command, i);
			buf[i] = alphabet[j];
			memcpy(buf + i + 1, command + i, strlen(command) - i);
			buf[strlen(command) + 1] = '\xff';
			buf[strlen(command) + 2] = '\0';
			spell_check_suggestion_search(command, buf, strlen(command) + 2);
		}
	}
}

static void help(void) {
	printf("%s [OPTIONS...] COMMAND ...\n\n"
		"Query commands not available.\n\n"
		"  -h --help                Show this help message\n"
		"     --ignore-installed    Do not suggest programs available locally\n\n",
		program_invocation_short_name);
}

static int parse_argv(int argc, char *argv[]) {

	enum {
		ARG_IGNORE_INSTALLED,
	};

	static const struct option options[] = {
		{ "help",                no_argument,       NULL, 'h'                     },
		{ "ignore-installed",    no_argument,       NULL, ARG_IGNORE_INSTALLED    },
		{}
	};

	int c;

	assert(argc >= 0);
	assert(argv);

	while ((c = getopt_long(argc, argv, "hH:M:", options, NULL)) >= 0)

		switch (c) {

		case 'h':
			help();
			exit(EXIT_SUCCESS);

		case ARG_IGNORE_INSTALLED:
			arg_ignore_installed = true;
			break;

		default:
			help();
			exit(EXIT_FAILURE);
		}

	arg_command = argv[argc - 1];
	return 1;
}

int main(int argc, char *argv[]) {
	int r;
	int fd = -1;
	char *command_ff, *bin, *package, **z, *s, *t, *t2, *t3, *component;
	FILE *sources_list;
	char buf[4096];
	char **prefixes = STRV_MAKE("/usr/bin", "/usr/sbin", "/bin", "/sbin",
			"/usr/local/bin", "/usr/games", NULL);
	struct stat st;
	bool is_main = false;

	/* run this early to prime the common case. */
	fd = open("/var/cache/command-not-found/db", O_RDONLY);

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (argc == 1) {
		errno = EINVAL;
		goto fail;
	}
	r = parse_argv(argc, argv);
	if (r <= 0) {
		return 1;
	}

	STRV_FOREACH(z, prefixes) {
		char w[strlen(*z) + 1 + strlen(arg_command) + 1];
		char *path;
		bool got_path = false;

		s = *z;

		r = snprintf(w, sizeof(w) + 1, "%s/%s", s, arg_command);
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
			if (r == 0 && path[strlen(s) + 1] != ':')
				got_path = true;
		} while ((path = strchrnul(path, ':') + 1));
		if (!got_path) {
			fprintf(stderr, _("The command could not be "\
"located because '%s' is not included in the PATH environment variable."), s);
			fputc('\n', stderr);
#ifndef CONFIG_OPENWRT
			if (strcmp(s + strlen(s) - 5, "sbin") == 0) {
				fprintf(stderr, _("This is most likely caused by the"\
" lack of administrative privileges associated with your user account."));
				fputc('\n', stderr);
			}
#endif
			return EXIT_SUCCESS;
		}

		if (arg_ignore_installed)
			break;

		fprintf(stderr, _("Command '%s' is available in '%s'"), arg_command,
			w);
		fputc('\n', stderr);
		return EXIT_SUCCESS;
	}

	if (fd < 0) {
#ifndef CONFIG_OPENWRT
		if (errno == ENOENT)
			fprintf(stderr, _("%s not found. "
				"Run 'update-command-not-found' as root.\n"),
				"/var/cache/command-not-found/db");
#endif
		goto fail;
	}

	r = fstat(fd, &st);
	if (r < 0)
		abort();
	file_size = st.st_size;
	file = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
	if (file == MAP_FAILED)
		goto fail;
	close(fd);

	// linux/binfmts.h:
	// #define MAX_ARG_STRLEN (PAGE_SIZE * 32)
	// so this will not overflow the stack
	command_ff = alloca(strlen(arg_command) + 2);
	strcpy(command_ff, arg_command);
	command_ff[strlen(arg_command)] = '\xff';
	command_ff[strlen(arg_command) + 1] = '\0';
	s = bisect_search(file, file_size, command_ff, strlen(arg_command) + 1);
	if (!s) {
		spell_check(arg_command);
		goto bail;
	}
	bin = strndupa(s, strchrnul(s, '\n') - s + 1);
	package = strchr(bin, '\xff');
	if (!package)
		goto bail;
	package++;
	component = strchrnul(package, '/');
	if (*component != '/') {
		*(component -1 ) = '\0';
		component = "main";
		is_main = true;
	} else {
		*component++ = '\0';
		*strchrnul(component, '\n') = '\0';
	}
	fprintf(stderr, _("The program '%s' is currently not installed. "), arg_command);
	if (is_root()) {
		fprintf(stderr, _("You can install it by typing:"));
#ifdef CONFIG_OPENWRT
		fprintf(stderr, "\nopkg install %s\n", package);
#elif HAVE_RPM
		fprintf(stderr, "\ndnf install %s\n", package);
#else
		fprintf(stderr, "\napt install %s\n", package);
#endif
	} else if (can_sudo() == 0) {
		fprintf(stderr, _("You can install it by typing:"));
#ifdef HAVE_RPM
		fprintf(stderr, "\nsudo dnf install %s\n", package);
#else
		fprintf(stderr, "\nsudo apt install %s\n", package);
#endif
	} else {
		fprintf(stderr, _("To run '%s' please ask your "
			"administrator to install the package '%s'"),
			arg_command, package);
		fputc('\n', stderr);
	}

	/* TODO, support suggesting feeds on OpenWRT */
#ifndef CONFIG_OPENWRT
	if (is_main)
#endif
		goto success;

	s = strv_find_prefix((char **)components, component);
	if (!s)
		goto success;

	// TODO investigate using 'apt-get indextargets' or <apt-pkg/sourcelist.h>
	// As long as this catches the default installed ubuntu, and changes via
	// Software and Updates...
	sources_list = fopen("/etc/apt/sources.list", O_RDONLY);
	while (fgets(&buf, sizeof(buf), sources_list) &&
	       strlen(&buf) > 4 &&
	       buf[0] == 'd' &&
	       buf[1] == 'e' &&
	       buf[2] == 'b' &&
	       (buf[3] == ' ' || buf[3] == '\t'))
		if (strstr(&buf, s))
			goto success;

	fprintf(stderr, _("You will have to enable "\
			"the component called '%s'"), s);
	fputc('\n', stderr);
success:
	return EXIT_SUCCESS;
fail:
	fputs(strerror(errno), stderr);
	fputc('\n', stderr);
	return EXIT_FAILURE;
bail:
	fprintf(stderr, _("%s: command not found"), arg_command);
	fputc('\n', stderr);
	return EXIT_SUCCESS;
}

