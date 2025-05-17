project('visiblegltf', [ 'c', 'cpp' ], default_options: ['c_std=c2x', 'cpp_std=c++20', 'warning_level=3'])

build_type = get_option('buildtype')

sdl3_dep = dependency('sdl3')

vendor_incdir = include_directories('vendor', is_system: true)

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

if host_machine.system() == 'darwin'
  vgltf_c_args += '-DVGLTF_PLATFORM_MACOS'
elif host_machine.system() == 'linux'
  vgltf_c_args += '-DVGLTF_PLATFORM_LINUX'
elif host_machine.system() == 'windows'
  vgltf_c_args += '-DVGLTF_PLATFORM_WINDOWS'
endif

vgltf_deps = [
  sdl3_dep,
  vulkan_dep,
]

vgltf_srcs = [
  'src/main.c',
  'src/log.c',
  'src/maths.c',
  'src/alloc.c',
  'src/hash.c',
  'src/str.c',
  'src/platform.c',
  'src/platform_sdl.c',
  'src/image.c',
  'src/engine.c',
  'src/graphics.c',
  'src/renderer/renderer.c',
  'src/renderer/vma_usage.cpp',
]

vgltf_exe = executable(
  'vgltf',
  vgltf_srcs,
  c_args: vgltf_c_args,
  dependencies: vgltf_deps,
  link_language: 'cpp',
  include_directories: [vendor_incdir]
)
