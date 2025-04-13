project(
  'nitrokit',
  'c',
  license: 'MIT',
  license_files: 'LICENSE',
  version: files('.version'),
  default_options: {
    'b_ndebug': 'if-release',
    'b_sanitize': 'address,undefined',
    'c_std': 'c99',
    'warning_level': '2',
    'werror': 'true',
  },
)

public_includes = include_directories('include')

clip_dep = declare_dependency(sources: files('source/clip.c'))

executable(
  'nitrorom',
  sources: files('source/main.c'),
  include_directories: public_includes,
  dependencies: [
    clip_dep,
  ],
)

subdir('tests')
