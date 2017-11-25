project('command-not-found', 'c',
        version : '0.4',
        license : 'LGPLv2+',
        default_options: [
                'c_std=gnu11',
                'prefix=/usr',
	],
)
executable('command-not-found', 'command-not-found.c', 'command-not-found.h',
	'util.c', 'util.h', 'bisect.c', 'bisect.h', install: true, install_dir : 'lib')
executable('update-command-not-found', 'update-command-not-found.c', 'update-command-not-found.h',
	'util.c', 'util.h', 'rb.c', 'rb.h', install_dir : 'sbin')

meson.add_install_script('po/install_translations.sh')
