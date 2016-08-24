if get_option('enable-builder')
  firmware200 = custom_target('firmware-2.0.0',
    output : 'colorhug-als-2.0.0.cab',
    input : [
      'firmware/2_0_0/firmware.inf',
      'firmware/2_0_0/firmware.bin'
    ],
    command : [gcab, '--create', '--nopath', '@OUTPUT@', '@INPUT@'],
  )

  firmware201 = custom_target('firmware-2.0.1',
    output : 'colorhug-als-2.0.1.cab',
    input : [
      'firmware/2_0_1/firmware.inf',
      'firmware/2_0_1/firmware.bin',
      'firmware/2_0_1/firmware.metainfo.xml'
    ],
    command : [gcab, '--create', '--nopath', '@OUTPUT@', '@INPUT@'],
  )

  firmware202 = custom_target('firmware-2.0.2',
    output : 'colorhug-als-2.0.2.cab',
    input : [
      'firmware/2_0_2/firmware.inf',
      'firmware/2_0_2/firmware.bin',
      'firmware/2_0_2/firmware.metainfo.xml'
    ],
    command : [gcab, '--create', '--nopath', '@OUTPUT@', '@INPUT@'],
  )
endif
