# Contributing to NitroROM

Thanks for your interest in contributing! This project is largely a solo-effort,
but collaborative effort is always appreciated. ❤️

Here, you'll find a loose set of guidelines for contributing to NitroROM. These
are not _rules_; use your best judgment and feel free to propose changes to this
document by way of a pull request.

## Table of Contents

- [Table of Contents](#table-of-contents)
- [How do I report a bug or a crash?](#how-do-i-report-a-bug-or-a-crash)
- [How do I set up a development environment?](#how-do-i-set-up-a-development-environment)
- [How do I run automated tests?](#how-do-i-run-automated-tests)
- [How do I submit an enhancement / bugfix?](#how-do-i-submit-an-enhancement-bugfix)

## How do I report a bug or a crash?

First, make sure that you have a debug-build of the program. You can verify this
by checking the version number of the program, which includes a `git` revision
hash when compiled as a debug-build:

```sh
./build/nitrorom --version
# e.g., 0.1.0+e1ed4af
```

From here, feel free to file a GitHub issue. In your issue-description, please
provide the following content:

- The version number (and ideally the revision number) of the built executable
- A crash report from any linked sanitizers
- Samples of your configuration files, if any
- The output of the program up to the crash-point when run with `--verbose`
- Ideally, a pointer to the offending code and a summary of the behavior, if
  easily identified

## How do I set up a development environment?

Meson will generate a `compile_commands.json` file in its build directory. To
make this discoverable by your LSP, you may need to create a symlink to that
file from your project folder:

```sh
ln -s build/compile_commands.json compile_commands.json
```

The repository provides some configuration for the following developer tools to
enforce some basic rules for code formatting, sanitation, and repository state:

- [`clang-tools`][clang-tools] - code formatting and static analysis
- [`editorconfig`][editorconfig] - standardized editor configuration
- [`pre-commit`][pre-commit] - enforce rules as part of commit workflows

Contributions to this repository must adhere to the standards enforced by these
tools.

[clang-tools]: https://clang.llvm.org/docs/ClangTools.html
[editorconfig]: https://editorconfig.org/
[pre-commit]: https://pre-commit.com/

## How do I run automated tests?

Meson comes with a built-in test harness. Code for the drivers is stored in the
`tests` folder as files prefixed with `test_`. The harness itself is configured
by the `test_suites` dictionary in `tests/meson.build`, which maps module-suites
to their driver and associated test cases.

To _run_ the full test-suite:

```sh
meson test -C build
```

Meson will report any failed expectations and emit a full log of the run in
`build/meson-logs/testlog.txt`.

## How do I submit an enhancement / bugfix?

Thanks for your effort to improve NitroROM! Please file a pull-request with your
proposed changes from a fork of this repository. Additionally, if your
contribution has an associated issue number, link to it in the description of
your pull request.

When describing your commit messages, please follow the [Conventional Commits
standard][conv-commits]. For a loose cheat-sheet summary, refer to [this
document][conv-commits-cheatsheet].

[conv-commits]: https://www.conventionalcommits.org/en/v1.0.0/
[conv-commits-cheatsheet]: https://gist.github.com/qoomon/5dfcdf8eec66a051ecd85625518cfd13
