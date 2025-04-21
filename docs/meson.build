man_1_adoc_files = [
  'nitrorom.adoc',
  'nitrorom-list.adoc',
  'nitrorom-pack.adoc',
]

asciidoctor_exe = find_program('asciidoctor')

man_1 = []
foreach man_1_adoc : man_1_adoc_files
  man_1_out = man_1_adoc.replace('.adoc', '.1')
  man_1 += custom_target(
    input: files(man_1_adoc),
    output: man_1_out,
    command: [
      asciidoctor_exe,
      '-b', 'manpage',
      '-a', 'manversion=' + meson.project_version(),
      '-o', '@OUTPUT@',
      '@INPUT@',
    ],
  )
endforeach

alias_target('docs', man_1)
