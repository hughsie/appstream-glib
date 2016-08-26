# Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
# Copyright (C) 2015 Igor Gnatenko <i.gnatenko.brain@gmail.com>
#
# Licensed under the GNU Lesser General Public License Version 2.1

pkgg = import('pkgconfig')

asbuilder_cargs = [
  '-DG_LOG_DOMAIN="Asb"',
  '-DASB_PLUGIN_DIR="@0@/@1@/asb-plugins-@2@"'.format(
    get_option('prefix'), get_option('libdir'),
    as_plugin_version,
  ),
]

deps = [glib, gmodule, gdkpixbuf, libarchive, soup, yaml]

if rpm.found()
  deps = deps + [rpm]
endif

if alpm.found()
  deps = deps + [alpm]
endif

headers = [
  'appstream-builder.h',
  'asb-app.h',
]

sources = [
  'asb-app.c',
  'asb-app.h',
  'asb-context.c',
  'asb-context.h',
  'asb-context-private.h',
  'asb-package.c',
  'asb-package-cab.c',
  'asb-package-cab.h',
  'asb-package-deb.c',
  'asb-package-deb.h',
  'asb-package.h',
  'asb-task.c',
  'asb-task.h',
  'asb-utils.c',
  'asb-utils.h',
  'asb-plugin.c',
  'asb-plugin.h',
  'asb-plugin-loader.c',
  'asb-plugin-loader.h',
]

if rpm.found()
  sources = sources + ['asb-package-rpm.c', 'asb-package-rpm.h']
endif

if alpm.found()
  sources = sources + ['asb-package-alpm.c', 'asb-package-alpm.h']
endif

mapfile = 'appstream-builder.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
asbuilder = shared_library(
  'appstream-builder', sources,
  soversion : lt_current,
  version : lt_version,
  dependencies : deps,
  c_args : asbuilder_cargs,
  include_directories : [
    top_build_incdir,
    asglib_incdir,
  ],
  link_args : vflag,
  link_depends : mapfile,
  link_with : asglib,
  install : true)
asbuilder_incdir = include_directories('.')

subdir('plugins')

pkgg.generate(
  version : as_version,
  libraries : asbuilder,
  name : 'appstream-builder',
  description : 'Objects and helper methods to help reading and writing AppStream metadata',
  filebase : 'appstream-builder',
  subddirs : 'appstream-builder')

asb_self_test = executable(
  'asb-self-test', 'asb-self-test.c',
  include_directories : [
    include_directories('..'),
    asglib_incdir,
  ],
  dependencies : [glib, gdkpixbuf, soup],
  c_args : cargs + [
    '-DTESTDIRSRC="@0@/../data/tests"'.format(meson.current_source_dir()),
    '-DTESTDIRBUILD="@0@/../data/tests"'.format(meson.current_build_dir()),
    '-DTESTPLUGINDIR="@0@/plugins"'.format(meson.current_build_dir()),
  ],
  link_with : asbuilder
)
test('asb-self-test', asb_self_test)

asbuilder_introspection_srcs = [
  'asb-app.c',
  'asb-app.h',
  'asb-context.c',
  'asb-context.h',
  'asb-context-private.h',
  'asb-package.c',
  'asb-package.h',
  'asb-task.c',
  'asb-task.h',
]

#gnome.generate_gir(asbuilder,
#  sources : asbuilder_introspection_srcs,
#  nsversion : '1.0',
#  namespace : 'AppStreamBuilder',
#  symbol_prefix : 'asb_',
#  identifier_prefix : 'Asb',
#  export_packages : 'appstream-glib',
#  includes : ['AppStreamGlib-1.0', 'GObject-2.0', 'Gio-2.0', 'GdkPixbuf-2.0'],
#  install : true
#)
