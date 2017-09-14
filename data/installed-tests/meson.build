# replace @bindir@ and @datadir@
con2 = configuration_data()
con2.set('bindir', join_paths(get_option('prefix'),
                              get_option('bindir')))
con2.set('datadir', join_paths(get_option('prefix'),
                               get_option('datadir')))

configure_file(
  input : 'appdata-validate.test.in',
  output : 'appdata-validate.test',
  configuration : con2,
  install: true,
  install_dir: join_paths('share', 'installed-tests', 'appstream-glib'),
)
configure_file(
  input : 'destdir-check.test.in',
  output : 'destdir-check.test',
  configuration : con2,
  install: true,
  install_dir: join_paths('share', 'installed-tests', 'appstream-glib'),
)

