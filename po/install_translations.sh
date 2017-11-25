#!/bin/sh
cd .. | exit
for dir in ../po/*.po
do
	dir=${dir%*/}
	[ -e "$dir" ] || continue
        LANGUAGE=`echo ${dir##*/} | sed 's/^command-not-found-\([^\.]*\).po$/\1/'`
#	echo $LANGUAGE
	mkdir -p "${DESTDIR}/${MESON_INSTALL_PREFIX}/share/locale/$LANGUAGE/LC_MESSAGES"
	msgfmt ../po/${dir##*/} -o "${DESTDIR}/${MESON_INSTALL_PREFIX}/share/locale/$LANGUAGE/LC_MESSAGES/command-not-found.mo"
#	echo $dir
done
