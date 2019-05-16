project('command-not-found', 'c',
        version : '0.4',
        license : 'LGPLv2+',
        default_options: [
                'c_std=gnu11',
                'prefix=/usr',
	],
)

add_project_arguments('-fPIE', language : 'c')
executable('command-not-found', 'command-not-found.c', 'command-not-found.h',
	'util.c', 'util.h', 'bisect.c', 'bisect.h', install: true, install_dir : 'lib')
executable('update-command-not-found', 'update-command-not-found.c', 'update-command-not-found.h',
	'util.c', 'util.h', install_dir : 'sbin')
install_data('zsh_command_not_found', install_dir : join_paths('/etc/', 'zsh_command_not_found'))
meson.add_install_script('po/install_translations.sh')
