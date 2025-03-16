project('visible-gltf', 'c', default_options: ['warning_level=3', 'c_std=c23'])

build_type = get_option('buildtype')

sdl3_dep = dependency('sdl3')
if host_machine.system() == 'darwin'
moltenvk_library_path = '/Users/clements/dev/VulkanSDK/1.4.309.0/macOS/lib'
moltenvk_include_path = '/Users/clements/dev/VulkanSDK/1.4.309.0/macOS/include'
vulkan_dep = declare_dependency(
  link_args: ['-L' + moltenvk_library_path, '-lvulkan'],
  include_directories: include_directories(moltenvk_include_path)
)
else 
vulkan_dep = dependency('vulkan')
endif

vgltf_c_args = []
if build_type == 'debug'
  vgltf_c_args += '-DVGLTF_DEBUG'
endif

executable(
  'vgltf',
  [
    'src/main.c',
    'src/log.c',
    'src/platform_sdl.c',
    'src/renderer.c',
  ],
  c_args: vgltf_c_args,
  dependencies: [sdl3_dep, vulkan_dep],
)
