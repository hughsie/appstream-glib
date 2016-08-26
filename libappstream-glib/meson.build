# Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
# Copyright (C) 2015 Igor Gnatenko <i.gnatenko.brain@gmail.com>
#
# Licensed under the GNU Lesser General Public License Version 2.1

pkgg = import('pkgconfig')

cargs = ['-DG_LOG_DOMAIN="As"', '-DLOCALSTATEDIR="/var"']

deps = [glib, gdkpixbuf, giounix, libarchive, soup, yaml, uuid, libgcab]

asresources = gnome.compile_resources(
  'as-resources', 'appstream-glib.gresource.xml',
  c_name : 'as')

configure_file(input : 'as-version.h.in',
               output : 'as-version.h',
               install_dir : get_option('includedir') + 'libappstream-glib',
               configuration : conf)

headers = [
  'appstream-glib.h',
  'as-app.h',
  'as-bundle.h',
  'as-checksum.h',
  'as-content-rating.h',
  'as-enums.h',
  'as-icon.h',
  'as-image.h',
  'as-inf.h',
  'as-markup.h',
  'as-monitor.h',
  'as-node.h',
  'as-problem.h',
  'as-profile.h',
  'as-provide.h',
  'as-release.h',
  'as-screenshot.h',
  'as-store.h',
  'as-tag.h',
  'as-translation.h',
  'as-utils.h']

sources = [
  'as-app-builder.c',
  'as-app-builder.h',
  'as-app.c',
  'as-app-desktop.c',
  'as-app-inf.c',
  'as-app-private.h',
  'as-app-validate.c',
  'as-bundle.c',
  'as-bundle.c',
  'as-bundle.h',
  'as-bundle-private.h',
  'as-bundle-private.h',
  'as-checksum.c',
  'as-checksum-private.h',
  'as-content-rating.c',
  'as-content-rating.h',
  'as-content-rating-private.h',
  'as-enums.c',
  'as-icon.c',
  'as-icon-private.h',
  'as-image.c',
  'as-image-private.h',
  'as-inf.c',
  'as-inf.h',
  'as-markup.c',
  'as-markup.h',
  'as-monitor.c',
  'as-monitor.c',
  'as-monitor.h',
  'as-monitor.h',
  'as-node.c',
  'as-node-private.h',
  'as-problem.c',
  'as-problem.h',
  'as-profile.c',
  'as-profile.h',
  'as-provide.c',
  'as-provide-private.h',
  'as-release.c',
  'as-release-private.h',
  asresources,
  'as-review.c',
  'as-review.h',
  'as-review-private.h',
  'as-screenshot.c',
  'as-screenshot-private.h',
  'as-stemmer.c',
  'as-stemmer.h',
  'as-store.c',
  'as-store-cab.c',
  'as-store-cab.h',
  'as-suggest.c',
  'as-suggest.h',
  'as-suggest-private.h',
  'as-tag.c',
  'as-translation.c',
  'as-translation.h',
  'as-translation-private.h',
  'as-utils.c',
  'as-utils-private.h',
  'as-yaml.c',
  'as-yaml.h']

if gperf.found()
  astagpriv = custom_target('gperf as-tag',
                            output : 'as-tag-private.h',
                            input : 'as-tag.gperf',
                            command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])
  #sources = sources + [astagpriv]
endif

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
  install : true)
asglib_incdir = include_directories('.')

pkgg.generate(
  version : as_version,
  libraries : asglib,
  name : 'appstream-glib',
  description : 'Objects and helper methods to help reading and writing AppStream metadata',
  filebase : 'appstream-glib',
  subddirs : 'appstream-glib')

selftest = executable(
  'as-self-test', 'as-self-test.c',
  include_directories : include_directories('..'),
  dependencies : deps,
  c_args : cargs + ['-DTESTDIRSRC="@0@/../data/tests"'.format(meson.current_source_dir())]
                 + ['-DTESTDIRBUILD="@0@/../data/tests"'.format(meson.current_build_dir())],
  link_with : asglib)
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
  'as-icon.c',
  'as-icon.h',
  'as-image.c',
  'as-image.h',
  'as-inf.c',
  'as-inf.h',
  'as-node.c',
  'as-node.h',
  'as-problem.c',
  'as-problem.h',
  'as-provide.c',
  'as-provide.h',
  'as-release.c',
  'as-release.h',
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
  'as-utils.h']

gnome.generate_gir(asglib,
  sources : introspection_sources,
  nsversion : '1.0',
  namespace : 'AppStreamGlib',
  symbol_prefix : 'as_',
  identifier_prefix : 'As',
  export_packages : 'appstream-glib',
  includes : ['GObject-2.0', 'Gio-2.0', 'GdkPixbuf-2.0'],
  install : true
)
