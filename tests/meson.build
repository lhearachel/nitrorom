test_clip = executable(
  'test_clip',
  sources: files('test_clip.c'),
  include_directories: public_includes,
  dependencies: [clip_dep],
)

test_config = executable(
  'test_config',
  sources: files('test_config.c'),
  include_directories: public_includes,
  dependencies: [config_dep],
)

# [suite -> { exe, [(name, args)...] }
test_suites = {
  'clip': {
    'exe': test_clip,
    'tests': [
      ['flag - short', ['-f', 'flag']],
      ['flag - long', ['--flag', 'flag']],
      ['string arg - short - next token', ['-s', 'value', 'string', 'value']],
      ['string arg - short - same token', ['-svalue', 'string', 'value']],
      ['string arg - long - next token', ['--string', 'value', 'string', 'value']],
      ['string arg - long - same token', ['--string=value', 'string', 'value']],
      ['numeric arg - short - next token', ['-n', '15', 'number', '15']],
      ['numeric arg - short - same token', ['-n15', 'number', '15']],
      ['numeric arg - long - next token', ['--number', '15', 'number', '15']],
      ['numeric arg - long - same token', ['--number=15', 'number', '15']],
      ['custom handler - short - next token', ['-h', 'custom', 'handle', 'custom']],
      ['custom handler - short - same token', ['-hcustom', 'handle', 'custom']],
      ['custom handler - long - next token', ['--handle', 'custom', 'handle', 'custom']],
      ['custom handler - long - same token', ['--handle=custom', 'handle', 'custom']],
      ['flag and string arg - combined short - next token', ['-fs', 'value', 'flag', 'string', 'value']],
      ['flag and string arg - combined short - same token', ['-fsvalue', 'flag', 'string', 'value']],
    ]
  },
  'config': {
    'exe': test_config,
    'tests': [
      ['ok', ['ok', files('config/ok.ini')]],
      ['missing key', ['nokey', files('config/nokey.ini')]],
      ['missing section title', ['nosec', files('config/nosec.ini')]],
      ['unterminated section header', ['untermsec', files('config/untermsec.ini')]],
      ['unrecognized section header', ['unknownsec', files('config/unknownsec.ini')]],
    ]
  },
}

foreach to_test, suite : test_suites
  runner = suite['exe']
  foreach params : suite['tests']
    test_name = params[0]
    test_args = params[1]
    test(test_name,
      runner,
      args: test_args,
      suite: [to_test],
    )
  endforeach
endforeach
