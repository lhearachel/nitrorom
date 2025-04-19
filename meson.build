project(
  'nitrokit',
  'c',
  license: 'MIT',
  license_files: 'LICENSE',
  version: files('.version'),
  default_options: {
    'c_std': 'c99',
    'warning_level': '2',
    'werror': 'true',
  },
)

if get_option('buildtype').startswith('debug')
  git_exe = find_program('git')
  revision_id = run_command(git_exe, 'rev-parse', '--short', 'HEAD', check: true).stdout().strip()
  revision_str = f'+@revision_id@'
else
  revision_str = ''
endif

config_data = configuration_data()
config_data.set_quoted('VERSION', meson.project_version())
config_data.set_quoted('REVISION', revision_str)
config_h = configure_file(output: 'config.h', configuration: config_data)

public_includes = include_directories('include')

libpng_dep = dependency('libpng')

clip_dep = declare_dependency(sources: files('source/libs/clip.c'))
strings_dep = declare_dependency(sources: files('source/libs/strings.c'))
config_dep = declare_dependency(sources: files('source/libs/config.c'), dependencies: [strings_dep])
sheets_dep = declare_dependency(sources: files('source/libs/sheets.c'), dependencies: [strings_dep])
fileio_dep = declare_dependency(sources: files('source/libs/fileio.c'), dependencies: [strings_dep])

executable(
  'nitrorom',
  sources: [
    files(
      'source/main.c',
      'source/packer.c',
      'source/parse/cfg_arm.c',
      'source/parse/cfg_banner.c',
      'source/parse/cfg_header.c',
      'source/parse/cfg_rom.c',
      'source/parse/csv_addfile.c',
    ),
    config_h,
  ],
  include_directories: public_includes,
  dependencies: [
    libpng_dep,
    clip_dep,
    config_dep,
    fileio_dep,
    sheets_dep,
    strings_dep,
  ],
)

subdir('tests')
