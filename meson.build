# Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
# Copyright (C) 2015 Igor Gnatenko <i.gnatenko.brain@gmail.com>
#
# Licensed under the GNU Lesser General Public License Version 2.1

project('appstream-glib', 'c')

as_major_version = '0'
as_minor_version = '6'
as_micro_version = '2'
as_version = '@0@.@1@.@2@'.format(as_major_version, as_minor_version, as_micro_version)

conf = configuration_data()
conf.set('AS_MAJOR_VERSION', as_major_version)
conf.set('AS_MINOR_VERSION', as_minor_version)
conf.set('AS_MICRO_VERSION', as_micro_version)
conf.set('PACKAGE_VERSION', '"@0@"'.format(as_version))

# this refers to the plugin API version
# this is not in any way related to a package or soname version
as_plugin_version = '2'

# libtool versioning - this applies to libpackagekit
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# - If interfaces have been changed or added, but binary compatibility
#   has been preserved, change:
#      CURRENT += 1
#      REVISION = 0
#      AGE += 1
# - If binary compatibility has been broken (eg removed or changed
#   interfaces), change:
#      CURRENT += 1
#      REVISION = 0
#      AGE = 0
# - If the interface is the same as the previous version, but bugs are
#   fixed, change:
#      REVISION += 1
lt_current = '7'
lt_revision = '5'
lt_age = '0'
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

uuid = dependency('uuid')
glib = dependency('glib-2.0')
gmodule = dependency('gmodule-2.0')
giounix = dependency('gio-unix-2.0')
libarchive = dependency('libarchive')
soup = dependency('libsoup-2.4', version : '>= 2.24')
gdk = dependency('gdk-3.0')
gdkpixbuf = dependency('gdk-pixbuf-2.0', version : '>= 2.14')
libgcab = dependency('libgcab-1.0', required : false)

# builder (default enabled)
if get_option('enable-builder')
  gtk = dependency('gtk+-3.0')
  gmodule = dependency('gmodule-2.0')
  gcab = find_program('gcab')
  if get_option('enable-rpm')
    rpm = dependency('rpm')
    conf.set('HAVE_RPM', 1)
  endif
  if get_option('enable-alpm')
    alpm = dependency('libalpm')
    conf.set('HAVE_ALPM', 1)
  endif
  if get_option('enable-fonts')
    conf.set('HAVE_FONTS', 1)
    freetype = dependency('freetype2', version : '>= 9.10.0')
    fontconfig = dependency('fontconfig')
  endif
endif
if get_option('enable-dep11')
  yaml = dependency('yaml-0.1')
  conf.set('AS_BUILD_DEP11', 1)
else
  yaml = dependency('yaml-0.1', required : false)
endif

# use gperf for faster string -> enum matching
gperf = find_program('gperf', required : false)

gnome = import('gnome')

if gperf.found()
  conf.set('HAVE_GPERF', 1)
endif
conf.set('GETTEXT_PACKAGE', '"appstream-glib"')
configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf)
top_build_incdir = include_directories(meson.current_build_dir())

add_global_arguments('-DAS_COMPILATION', language : 'c')
add_global_arguments('-DLOCALEDIR="@0@"'.format(get_option('localedir')), language : 'c')

subdir('libappstream-glib')
subdir('data')

if get_option('enable-builder')
  # depends on data
  subdir('libappstream-builder')
endif

# depends on libappstream-builder
subdir('client')

archiver = find_program('git', required : false)
if archiver.found()
  run_target('dist', 'git', 'archive', '--prefix=appstream-glib/',
             'HEAD',
             '--output', 'appstream-glib-' + as_version + '.tar.gz')

#-version=`./meson.py -v`
#-git archive --prefix meson-${version}/ HEAD | gzip > meson_${version}.tar.gz


else
  message('git-archive-all not found, you will not be able to run `ninja dist`')
  message('Install git-archive-all with pip:')
  message('   $ pip install git-archive-all')
endif
