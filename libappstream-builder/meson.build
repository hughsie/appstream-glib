pkgg = import('pkgconfig')

asbuilder_cargs = [
  '-DG_LOG_DOMAIN="Asb"',
  '-DASB_PLUGIN_DIR=' + '"' + plugindir + '"',
]

deps = [
  glib,
  gmodule,
  gdkpixbuf,
  libarchive,
  soup,
]

if get_option('dep11')
  deps = deps + [yaml]
endif

if get_option('rpm')
  deps = deps + [rpm]
endif

if get_option('alpm')
  deps = deps + [alpm]
endif

headers = [
  'appstream-builder.h',
  'asb-app.h',
]

install_headers(headers, subdir : 'libappstream-builder')

sources = [
  'asb-app.c',
  'asb-context.c',
  'asb-package.c',
  'asb-package-cab.c',
  'asb-package-deb.c',
  'asb-task.c',
  'asb-utils.c',
  'asb-plugin.c',
  'asb-plugin-loader.c',
]

if get_option('rpm')
  sources = sources + ['asb-package-rpm.c']
endif

if get_option('alpm')
  sources = sources + ['asb-package-alpm.c']
endif

top_build_incdir = include_directories('..')

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
  install : true,
)
asbuilder_incdir = include_directories('.')

subdir('plugins')

pkgg.generate(
  version : as_version,
  libraries : asbuilder,
  requires : [
    'glib-2.0',
    'gobject-2.0',
    'gdk-pixbuf-2.0',
  ],
  name : 'appstream-builder',
  description : 'Objects and helper methods to help reading and writing AppStream metadata',
  filebase : 'appstream-builder',
  subdirs : 'libappstream-builder'
)

asb_self_test = executable(
  'asb-self-test',
  'asb-self-test.c',
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
  link_with : [
    asbuilder,
    asglib,
  ],
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

if get_option('introspection')
  asglib_gir_dep = declare_dependency(sources: asglib_gir)

  gnome.generate_gir(asbuilder,
    sources : asbuilder_introspection_srcs,
    nsversion : '1.0',
    namespace : 'AppStreamBuilder',
    symbol_prefix : 'asb_',
    identifier_prefix : 'Asb',
    export_packages : 'appstream-builder',
    dependencies: asglib_gir_dep,
    includes : [
      'AppStreamGlib-1.0',
      'GdkPixbuf-2.0',
      'Gio-2.0',
      'GObject-2.0',
    ],
    install : true
  )
endif
