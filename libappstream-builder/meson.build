pkgg = import('pkgconfig')

asbuilder_cargs = [
  '-DG_LOG_DOMAIN="Asb"',
  '-DASB_PLUGIN_DIR=' + '"' + plugindir + '"',
]

deps = [
  gio,
  gmodule,
  gdkpixbuf,
  libarchive,
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

asbuilder = static_library(
  'appstream-builder', sources,
  dependencies : deps,
  c_args : asbuilder_cargs,
  include_directories : [
    top_build_incdir,
    asglib_incdir,
  ],
  link_with : asglib,
)
asbuilder_incdir = include_directories('.')

subdir('plugins')

asb_self_test = executable(
  'asb-self-test',
  'asb-self-test.c',
  include_directories : [
    include_directories('..'),
    asglib_incdir,
  ],
  dependencies : [gio, gdkpixbuf],
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
