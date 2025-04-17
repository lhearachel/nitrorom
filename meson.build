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

genconfig_data = configuration_data()
genconfig_data.set_quoted('VERSION', meson.project_version())
genconfig = configure_file(output: 'genconfig.h', configuration: genconfig_data)

public_includes = include_directories('include')

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
    genconfig,
  ],
  include_directories: public_includes,
  dependencies: [
    clip_dep,
    config_dep,
    fileio_dep,
    sheets_dep,
    strings_dep,
  ],
)

subdir('tests')
