# Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
#
# Licensed under the GNU Lesser General Public License Version 2.1

if get_option('enable-builder')

custom_target('firmware-2.0.0',
output : 'colorhug-als-2.0.0.cab',
input : ['firmware/2_0_0/firmware.inf',
         'firmware/2_0_0/firmware.bin'],
command : [gcab, '--create', '--nopath', '@OUTPUT@', '@INPUT@'],
install : false,
install_dir : ''
)

custom_target('firmware-2.0.1',
output : 'colorhug-als-2.0.1.cab',
input : ['firmware/2_0_1/firmware.inf',
         'firmware/2_0_1/firmware.bin',
         'firmware/2_0_1/firmware.metainfo.xml'],
command : [gcab, '--create', '--nopath', '@OUTPUT@', '@INPUT@'],
install : false,
install_dir : ''
)

custom_target('firmware-2.0.2',
output : 'colorhug-als-2.0.2.cab',
input : ['firmware/2_0_2/firmware.inf',
         'firmware/2_0_2/firmware.bin',
         'firmware/2_0_2/firmware.metainfo.xml'],
command : [gcab, '--create', '--nopath', '@OUTPUT@', '@INPUT@'],
install : false,
install_dir : ''
)

endif
