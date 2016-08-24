# Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
# Copyright (C) 2015 Igor Gnatenko <i.gnatenko.brain@gmail.com>
#
# Licensed under the GNU Lesser General Public License Version 2.1

install_data('appstream-xml.m4', install_dir : 'share/aclocal')

install_man('appstream-util.1')

if get_option('enable-builder')
  install_man('appstream-builder.1')
endif

subdir('installed-tests')
subdir('tests')
