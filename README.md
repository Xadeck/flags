# Minimalist C++20 library for command line flags parsing

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![GitHub Actions: CMake](https://github.com/Xadeck/flags/workflows/CMake%20on%20multiple%20platforms/badge.svg)](https://github.com/Xadeck/flags/actions/workflows/cmake-multi-platform.yml)
[![GitHub Actions: Bazel](https://github.com/Xadeck/flags/workflows/Bazel%20on%20multiple%20platforms/badge.svg)](https://github.com/Xadeck/flags/actions/workflows/bazel-multi-platform.yml)


This library is for you if:

* you need to parse command line arguments in your project,
* you want a simple syntax and lightweight library,
* you can use C++20 in your project,

An example is worth a thousand words. Here is the syntax for defining and
parsing flags.

```c++ example.cc
#include "xdk/flags/flags.h"

namespace {
constexpr std::string_view kHelp = R"(

Runs a server on the given port (default is 8080).

  --port    : specify the port to use.
  --help/-h : prints this help.
)";
}

int main(int argc, char** argv) {
  struct Flags : xdk::Flags<Flags> {
    Flag<"--port", int>        port{8080};
    Flag<"--help", bool, "-h"> help;
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

  std::cout << "Running on port " << flags.port.value << `\n';
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

* Supports long/short forms for flags e.g. `--long/-l` via aliases.
* Supports repeated flags, e.g. `-l one -l two -l three` as a `{"one", "two", "three"}`.
* Supports optional flags.
* Supports default flag values.
* Supports `--` to stop flag parsing
* Reports invalid values that can't be streamed into the flag's type.
* Reports missing flag values.

The following other typical features flags are **not** supported:

* No mechanism for help strings.
* No mechanism for complex validation.
* No mechanism for commands and subcommands.
* No support for merging of flags, e.g. `-l -t` written as `-lt`.
* No support for `=` i.e. `--flag=value`.

If you want those features, you need powerful - but more complex - library such
as [CLI11](https://github.com/CLIUtils/CLI11).

### About help strings

Displaying help strings automatically is not an easy task. Nice documentation
must be nicely formatted, which depend on the terminal's width.  Documentation
may be localized. This is better left to dedicated tools, like man pages.

For simple program, you can simply maintain the documentation as a string in
your main file. You can use an ordered map associating flag names to their
description.  If you want to make sure it contains an entry for each flags, you
can use the introspection API.

The introspection API is also useful for producing a list of completions for
shell integration of your binary.

### About validation

The `Flags::Parse` method only reports when:
1. it can not parse a command line argument into a flag's type;
2. it can not find a value for a non-boolean flag.

For example, if you have a `Flag<"--port", int>`, and you try to parse:
1. `--port not_an_int` or
2. `--port`
2. `--port --other_flag`

Then you will get a parsing error.

There is no other validation, such as mandatory flags, checking that a value is
in a range, or that some flags are mutually exclusive.  Such validation is best
performed by you directly in code, at the beginning of the program. As for help
strings above, this also allows for localized error reporting.

## Installation

Assuming you are using [Bazel](http://bazel.build), add the following to your
`MODULE.bzl`. Make sure to update to [the latest
commit](https://github.com/Xadeck/flags/commits/main).

```bzl
bazel_dep(name = "xdk_flags")
git_override(
    module_name = "xdk_flags",
    commit = "34a5c32ae3a4db2ee956ec5a1bfa2d1c46751d8a",
    remote = "https://github.com/Xadeck/flags.git",
)
```

You also need to enable C++20 for compilation. Unless you are comfortable
setting a toolchain, the easiest way to do so is to add the following to your
`.bazelrc`:

```bzl
build --cxxopt=-std=c++20
```

## Usage

The library is of the opinion that flags should only be defined as the entry
point of a binary, in its `main()` function. Mechanism like [Abseil
flags](https://abseil.io/docs/python/guides/flags) are convenient for deep
dependency injection but they come at a maintenance cost. This library doesn't
try to wade in those waters, so we'll assume you have a `cc_binary` with one
source defining a `main()` function.  In your `BUILD` file, add a dependency to
this library to the binary, for example:

```bzl
cc_binary(
  name = "server",
  srcs = ["server.cc"],
  deps = [
    "@xdk_flags//xdk/flags",
    # ...
  ],
)
```

In your main file, here `server.cc`, include the header:

```c++
#include "xdk/flags/flags.h"
```

### Flags definition

In your code, define a struct that inherits from `xdk::Flags` -notice the
plural- using itself as the template parameter, a pattern known as
[CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern). You
can call it `Flags` too if you want, and you can define it inside your main
function, or in the anonymous namespace.

```c++
struct Flags : xdk::Flags<Flags> {
  // ...
};
```

That struct *must* have only fields of the `Flag` template type [^3] -notice
the singular- whose first parameter is a string matching what will be used on
the command line, and second parameter is a type.

[^3]: not respecting this constraint is undefined behavior, most likely a
    crash.

```c++
struct Flags : xdk::Flags<Flags> {
  Flag<"--port", int> port;
  // ...
};
```

The string must not be empty, must start with a `-` and not be the exact string
`--`. This is enforced at compile-time with a static assert.  The name of the
field does not need to match the flag, so you can use a descriptive variable
name, and a short flag name.

```c++
struct Flags : xdk::Flags<Flags> {
  Flag<"-p", int> port;
  // ...
};
```

You can use any moveable type that supports `operator>>(std::istream&)`, any
`std::vector` of such a type, and any `std::optional` of such a type. Non
copyable and non default constructible types are usable.

The `Flag` class accepts an optional third template parameter, which is also a
string, must also not be empty, start with a `-` and not be the exact string
`--`.  This can be used to specify an *alias* for the flag. The typical
usage is to define a short version of the flag:

```c++
struct Flags : xdk::Flags<Flags> {
  Flag<"--port", int, "-p"> port;
  // ...
};
```

If not specified, this parmater defaults to the value of the first parameter.

Finally, the `Flag` constructor supports all constructors for the associated
type, so you can initialize the field with a default value:

```c++
struct Flags : xdk::Flags<Flags> {
  Flag<"--center", std::pair<float, float>> center{1.f, 5.f};
  // ...
};
```

### Command line parsing

Once you have defined your `Flags` class, you can use its `Parse()` method to
get an instance, initialized from command line arguments. It returns a tuple of
3 elements, that you typically destructure like this:

```c++
int main(int arc, char** argv) {
  // ...
  auto [flags, args, errors] = Flags::Parse(argc, argv);
```

The first element is an instance of your `Flags` class. The second element is a
`std::vector<const char*>`.   The third element is akin to a vector of error
tuples, which will be described later.  Parsing *always* returns an instance of
your `Flags` type.

* If `errors` is empty, the fields of `flags` have all been properly
  initialized from the command line arguments, and `args` contains all the
arguments that were neither flag names nor flag values. If your application
doesn't support positional arguments, simply report and error if `args` is not
empty.
* If `errors` is not empty, flags are invalid, and the corresponding fields
  have an undefined value. You must report errors to the user, as described
later on this page.

### Flags usage

Once you have the `flags` instance, you access the values of command line
arguments simply through its fields, via the `value` accessor. `Flag<>`
implicitly converts to the underlying type, and supports the `->` operator, so
all forms below are valid:

```c++
  struct Flags : xdk::Flags {
    Flag<"--file", std::string> file;
  };
  auto [flags, _, errors] = Flags::Parse(argc, argv);

  std::string s = flags.file.value;    // explicit access of value
  std::string t = flags.file;          // implicit conversion to underlying type
  size_t l      = flags.file->size();  // access of members via ->
```

Flag values can be modified, but we recommend treating them as constants.

### Patterns

#### Repeated flags

If a flag is of type `std::vector<T>` where `T` is moveable and supports
`operator>>(std::istream&`, then it will contain an element per instance of the
flag on the command line. That is, if you have:

```c++
struct Flags : xdk::Flags<Flags> {
  Flag<"--value", std::vector<int>, "-v"> values;
};
```

Then parsing `--value 2 -v 1 - v 3` will produce `flags.values = {2,1,3}`. Interestingly, you
can sort the vector in place:

#### Optional flags

If a flag *must* be specified on the command line, because there is no
reasonable default value, use a `std::optional` for the flag's type, and check
that is has a value.

If you need, to be able to determine if a flag was specified in the command
line, and yet provide a default value, then use the `std::optional::value_or`
method, as in the example below.

```c++
struct Flags : xdk::Flags<Flags> {
  Flag<"--port", std::optional<int>, "-p"> port;
  // ...
};

auto [flags, _, errors] = Flags::Parse(argc, argv);
if (!flags.port.has_value()) std::cout << "Using default port 8080.\n";

int port = flags.port.value_or(8080);
```

If you can use C++ 23's
[`or_else`](https://en.cppreference.com/w/cpp/utility/optional/or_else), you
can combine the above into:

```c++
int port = flags.port.or_else([]() {
   std::cout << "Using default port 8080.\n";
   return 8080;
});
```

### Reporting errors.

When parsing the flags as follows:

```c++
int main(int arc, char** argv) {
  // ...
  auto [flags, args, errors] = Flags::Parse(argc, argv);
```

The `errors` object is akin to an `std::vector` with a convenient
conversion-to-boolean operator, and output-to-stream operator. This allows for
a minimal approach to report errors.

```c++
  if (errors) { // of `if (!errors.empty()) `
    std::cerr << "Invalid arguments:\n" << errors;
    return EXIT_FAILURE;
  }
```

For a custom reporting of errors, traverse the `errors` struct, which has 3
fields `pos`, `arg` and `val`. The last one distinguishes the error cases:

1. unknown flag: `val` equals to `FlagInfo::Error::kUnknown`
2. invalid flag value: `val` equals to a string that can't be parsed into the given flag's type
3. missing flag value: `val` equals to `nullptr`

In each case, `pos` is the index in `argv` of the flag causing an error, and
`arg` is `argv[pos]`.

### Ignoring unkwnown flags.

The `Parse()` method takes an optional third argument to indicate that unknown
flags are not errors, and are simply added to `args` instead.

### Multiple flag classes.

This pattern is useful to implement a sub-command mechanism - e.g. `git status`
or `git commit`, where the first element of `args` is a command. In such a
case, you have flags for the main command (`git`) and flags for each possible
sub-command (`status` or `commit`).

To handle such a case, use multiple `Flags` classes, one for the main command
and one for each sub-command. Parse `argv` using the first one and ignoring
unknown flags. Then identify the subcommand using `args[0]`, and parse the
returned `args` object with its flags, updating the existing `args` and
`errors`. Here is a pseudo-code, illustrating the approach:

```c++
 GitFlags : Flags<GitFlags> {
    Flag<"-v", bool> verbose{false};
  };

  auto [git, args, errors] = GitFlags::Parse(argv, false);

  if (args[0] == "status"sv) {
    struct StatusFlags : Flags<StatusFlags> {
      Flag<"-s", bool> short_format{false};
    };
    auto status = StatusFlags::Parse(args, errors);
    // ..
    // use `status.short_format` and `git.verbose` flags
    // ..
  }

  if (args[0] == "commit"sv) {
    struct CommitFlags : Flags<CommitFlags> {
      Flag<"-a", bool> all{false};
    };
    auto commit = CommitFlags::Parse(args, errors);
    // ..
    // use `commit.all` and `git.verbose` flags
    // ..
  }
```

## Introspection API

You can use the `Flags::FlagInfos()` method on your `Flags` type to get a
vector of `FlagInfo` objects, which are structs with 3 fields: `name`, `alias`
and `type`. The first two ones are clear. The last one is a
[`std::type_info`](https://en.cppreference.com/w/cpp/types/type_info) pointer
describing a flag's underlying type.

This API allows you to store documentation for command line flags into a map,
or a set of external files, or what ever you like, and implement a test that
all flags have a corresponding docstring, and conversely.
