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

  // ...

  return EXIT_SUCCESS;
}
