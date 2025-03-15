project('visible-gltf', 'c', default_options: ['warning_level=3', 'c_std=c23'])

sdl3_dep = dependency('sdl3')

executable(
  'vgltf',
  [
    'src/main.c',
    'src/log.c',
    'src/platform_sdl.c'
  ],
  dependencies: [sdl3_dep]
)
