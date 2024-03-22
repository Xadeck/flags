# Minimalist C++ library for command line flags parsing

An example is worth a thousand words:

```c++ example
#include "xdk/flags/flags.h"

namespace {
constexpr std::string_view kHelp = R"(

Runs a server on the given port (default is 8080).

  --port    : specify the port to use.
  --help/-h : prints this help.
)";
}

int main(int argc, char** argv) {
  struct Flags : xdk::Flags<MyFlags> {
    Flag<"--port", int>       port{8080};
    Flag<"--help, bool, "-h"> help;
  };

  auto [flags, args, errors] = Flags::Parse(argc, argv);
  if (errors) {
    std::cerr << "Invalid arguments:\n" << errors;
    return EXIT_FAILURE;
  }
  if (args.size() > 1) {
    std::cerr << args[0] << " doesn't take any argument.\n";
    return EXIT_FAILURE;
  }
  if (flags.help) {
    std::cout << args[0] << "\n"              //
              << args[0] << " --port 8080\n"  //
              << kHelp.substr(1);             // skip first newline.
    return EXIT_SUCCESS;
  }

  // ...

  return EXIT_SUCCESS;
}
```

Flags are simple C++ object whose names are unconstrained by the flag's name or
alias. They can be declared locally in the `main()` function. There are no
macros involved. Validation and error reporting is explicit, as well as help
string.

## Features

The following typical features for command line flags are supported:

* Supports long/short forms for flags e.g. `--long/-l` via aliases [^1].
* Supports repeated flags.
* Supports optional flags.
* Supports default flag values.
* Supports `--` to stop flag parsing
* Reports invalid values that can be streamed into the flag's type [^2].
* Reports missing flag values.

[^1]: aliases are more general.
[^2]: special case for `char`.

The following other features flags are **not** supported:

* No mechanism for help strings.
* No mechanism for validation.
* No mechanism for commands and subcommands.
* No support for merging of flags, e.g. `-l -t` written as `-lt`.
* No support for `=` i.e. `--flag=value`.

If you want those features, you need powerful - but more complex - library such
as [CLI11](https://github.com/CLIUtils/CLI11).

## About help strings

Displaying help strings automatically is not an easy task. Nice documentation
must be nicely formatted, which depend on the terminal's width.  Documentation
may be localized. This is better left to dedicated tools, like man pages.

For simple program, you can simply maintain the documentation as a string in
your main file. You can use an ordered map associating flag names to their
description.  If you want to make sure it contains an entry for each flags, you
can use the introspection API.

That can be useful for producing a list of completions.

## About validation

The `Flags::Parse` method only reports when:
1. it can not parse a command line argument into a flag's type;
2. it can not find a value for a non-boolean flag.

For example, if you have a `Flag<"--port", int>`, and you try to parse:
1. `--port not_an_int` or
2. `--port`
2. `--port --other_flag`

There is no other validation, such as mandatory flags, checking that a value is
in a range, or that some flags are mutually exclusive.  Such validation is best
performed by you directly in code, at the beginning of the program. As for help
strings above, this also allows for localized error reporting.

## Installation

TODO:

## Usage

TODO:

### Patterns

If a flag must be specified on the command line, because there is no reasonable
default value, use a `std::optional` for the flag's type, and check that is has
a value.

Reporting errors.

Ignoring unkwnown flags.

Re-usable flag classes.

### Anti-patterns

Passing flags to libraries.
