{
  'targets': [
    {
      'target_name': 'gif',
      'sources': [
        'src/animated_gif.cpp',
        'src/async_animated_gif.cpp',
        'src/common.cpp',
        'src/dynamic_gif_stack.cpp',
        'src/gif.cpp',
        'src/gif_encoder.cpp',
        'src/module.cpp',
        'src/palette.cpp',
        'src/quantize.cpp',
        'src/utils.cpp'
      ],
      "include_dirs" : ["<!(node -p -e \"require('path').dirname(require.resolve('nan'))\")"],
      "libraries": ["-lgif"],
      'cflags_cc!': [ '-fno-exceptions' ],
      'conditions': [
          ['OS=="mac"', {
              'xcode_settings': {
                  'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
              }
          }]
      ]
    }
  ]
}
