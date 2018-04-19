pkgg = import('pkgconfig')

cargs = [
  '-DG_LOG_DOMAIN="As"',
]

deps = [
  gdkpixbuf,
  giounix,
  glib,
  libarchive,
  libgcab,
  soup,
  uuid,
]

if get_option('dep11')
  deps += yaml
endif

if get_option('stemmer')
  deps += stemmer
endif

asresources = gnome.compile_resources(
  'as-resources', 'appstream-glib.gresource.xml',
  c_name : 'as'
)

configure_file(
  input : 'as-version.h.in',
  output : 'as-version.h',
  install_dir : join_paths(get_option('includedir'), 'libappstream-glib'),
  configuration : conf
)

headers = [
  'appstream-glib.h',
  'as-app-builder.h',
  'as-app.h',
  'as-bundle.h',
  'as-checksum.h',
  'as-content-rating.h',
  'as-enums.h',
  'as-format.h',
  'as-icon.h',
  'as-image.h',
  'as-launchable.h',
  'as-inf.h',
  'as-markup.h',
  'as-monitor.h',
  'as-node.h',
  'as-agreement.h',
  'as-agreement-section.h',
  'as-problem.h',
  'as-profile.h',
  'as-provide.h',
  'as-release.h',
  'as-require.h',
  'as-review.h',
  'as-screenshot.h',
  'as-store.h',
  'as-suggest.h',
  'as-tag.h',
  'as-translation.h',
  'as-utils.h',
]

sources = [
  'as-app-builder.c',
  'as-app.c',
  'as-app-desktop.c',
  'as-app-inf.c',
  'as-app-validate.c',
  'as-bundle.c',
  'as-bundle.c',
  'as-checksum.c',
  'as-content-rating.c',
  'as-enums.c',
  'as-format.c',
  'as-icon.c',
  'as-image.c',
  'as-inf.c',
  'as-launchable.c',
  'as-markup.c',
  'as-monitor.c',
  'as-monitor.c',
  'as-node.c',
  'as-agreement.c',
  'as-agreement-section.c',
  'as-problem.c',
  'as-profile.c',
  'as-provide.c',
  'as-ref-string.c',
  'as-release.c',
  'as-require.c',
  'as-review.c',
  'as-screenshot.c',
  'as-stemmer.c',
  'as-store.c',
  'as-store-cab.c',
  'as-suggest.c',
  'as-tag.c',
  'as-translation.c',
  'as-utils.c',
  'as-version.c',
  'as-yaml.c',
  asresources,
]

# gperf sources
astagpriv = custom_target(
  'gperf as-tag',
  output : 'as-tag-private.h',
  input : 'as-tag.gperf',
  command : [
    gperf,
    '@INPUT@',
    '--output-file',
    '@OUTPUT@'
  ]
)
sources = sources + [astagpriv]

install_headers(headers, subdir : 'libappstream-glib')

mapfile = 'appstream-glib.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
asglib = shared_library(
  'appstream-glib', sources,
  soversion : lt_current,
  version : lt_version,
  dependencies : deps,
  c_args : cargs,
  include_directories : include_directories('..'),
  link_args : vflag,
  link_depends : mapfile,
  install : true,
)
asglib_incdir = include_directories('.')

pkgg.generate(
  version : as_version,
  libraries : asglib,
  requires : [
    'glib-2.0',
    'gobject-2.0',
    'gdk-pixbuf-2.0',
  ],
  requires_private : [
    'libarchive',
    'uuid',
    'libgcab-1.0',
  ],
  name : 'appstream-glib',
  description : 'Objects and helper methods to help reading and writing AppStream metadata',
  filebase : 'appstream-glib',
  subdirs : 'libappstream-glib',
)

selftest = executable(
  'as-self-test', 'as-self-test.c',
  include_directories : include_directories('..'),
  dependencies : deps,
  c_args : cargs + ['-DTESTDIRSRC="@0@/../data/tests"'.format(meson.current_source_dir())]
                 + ['-DTESTDIRBUILD="@0@/../data/tests"'.format(meson.current_build_dir())],
  link_with : asglib,
)
test('as-self-test', selftest)

introspection_sources = [
  'as-app.c',
  'as-app.h',
  'as-app-validate.c',
  'as-bundle.c',
  'as-bundle.h',
  'as-checksum.c',
  'as-checksum.h',
  'as-content-rating.c',
  'as-content-rating.h',
  'as-enums.c',
  'as-enums.h',
  'as-format.c',
  'as-format.h',
  'as-icon.c',
  'as-icon.h',
  'as-image.c',
  'as-image.h',
  'as-inf.c',
  'as-inf.h',
  'as-launchable.c',
  'as-launchable.h',
  'as-markup.c',
  'as-markup.h',
  'as-node.c',
  'as-node.h',
  'as-agreement.c',
  'as-agreement.h',
  'as-agreement-section.c',
  'as-agreement-section.h',
  'as-problem.c',
  'as-problem.h',
  'as-provide.c',
  'as-provide.h',
  'as-release.c',
  'as-release.h',
  'as-require.c',
  'as-require.h',
  'as-review.c',
  'as-review.h',
  'as-screenshot.c',
  'as-screenshot.h',
  'as-store.c',
  'as-store.h',
  'as-suggest.c',
  'as-suggest.h',
  'as-tag.c',
  'as-tag.h',
  'as-translation.c',
  'as-translation.h',
  'as-utils.c',
  'as-utils.h',
  'as-version.c',
]

if get_option('introspection')
  asglib_gir = gnome.generate_gir(asglib,
    sources : introspection_sources,
    nsversion : '1.0',
    namespace : 'AppStreamGlib',
    symbol_prefix : 'as_',
    identifier_prefix : 'As',
    export_packages : 'appstream-glib',
    dependencies : deps,
    includes : [
      'GObject-2.0',
      'Gio-2.0',
      'GdkPixbuf-2.0'
    ],
    link_with : asglib,
    install : true,
  )
endif
