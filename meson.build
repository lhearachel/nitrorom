project(
  'nitrorom',
  'c',
  license: 'MIT',
  license_files: 'LICENSE',
  meson_version: '>=1.2.0',
  version: files('.version'),
  default_options: {
    'c_std': 'c99',
    'buildtype': 'release',
    'warning_level': '2',
    'werror': 'true',
  },
)

native = get_option('native')
install = native and meson.is_cross_build() ? false : true

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

libpng_dep = dependency('libpng', native: native)

clip_dep = declare_dependency(sources: files('source/libs/clip.c'))
strings_dep = declare_dependency(sources: files('source/libs/strings.c'))
config_dep = declare_dependency(sources: files('source/libs/config.c'), dependencies: [strings_dep])
sheets_dep = declare_dependency(sources: files('source/libs/sheets.c'), dependencies: [strings_dep])
fileio_dep = declare_dependency(sources: files('source/libs/fileio.c'), dependencies: [strings_dep])

nitrorom_exe = executable(
  'nitrorom',
  sources: [
    files(
      'source/nitrorom.c',
      'source/nitrorom_list.c',
      'source/nitrorom_pack.c',
      'source/packer.c',
      'source/parse/cfg_arm.c',
      'source/parse/cfg_banner.c',
      'source/parse/cfg_header.c',
      'source/parse/cfg_rom.c',
      'source/parse/csv_addfile.c',
    ),
    config_h,
  ],
  native: native,
  install: install,
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

if not meson.is_subproject()
  subdir('tests')
  if get_option('manuals')
    subdir('docs')
  endif
else
  meson.override_find_program('nitrorom', nitrorom_exe)
endif
