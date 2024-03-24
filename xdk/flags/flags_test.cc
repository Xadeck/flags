#include "xdk/flags/flags.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace xdk {
std::ostream& operator<<(std::ostream& os, const FlagInfo::Error& error) {
  os << "FlagInfo::Error{.pos=" << error.pos;
  if (error.arg != nullptr) os << ", .arg=" << std::quoted(error.arg);
  if (error.val != nullptr) os << ", .val=" << std::quoted(error.val);
  return os << "}";
}

namespace {
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::StrEq;

TEST(FlagsTest, ValidFlagInfoStrings) {
  ASSERT_FALSE(FlagInfo::String("").IsValid());
  ASSERT_FALSE(FlagInfo::String("a").IsValid());
  ASSERT_FALSE(FlagInfo::String("--").IsValid());

  ASSERT_TRUE(FlagInfo::String("-s").IsValid());
  ASSERT_TRUE(FlagInfo::String("--long").IsValid());

  ASSERT_TRUE(FlagInfo::String("-").IsValid());
  ASSERT_TRUE(FlagInfo::String("---").IsValid());
  ASSERT_TRUE(FlagInfo::String("-a-b_c").IsValid());
}

// To test that flags of non default-constructible, move-only type work.
struct Point {
  Point(float x, float y) : x(x), y(y) {}
  Point(const Point&) = delete;
  Point(Point&&)      = default;

  float x;
  float y;

  friend std::istream& operator>>(std::istream& is, Point& p) {
    return is >> p.x >> p.y;
  }
};

TEST(FlagsTest, SimpleDemo) {
  struct TestFlags : Flags<TestFlags> {
    Flag<"--path", std::string>                 path;
    Flag<"--port", int, "-p">                   port{3};
    Flag<"--verbose", bool>                     verbose;       // boolean  flag
    Flag<"--fruits", std::vector<std::string>>  fruits;        // repeated flag
    Flag<"--drink", std::optional<std::string>> drink;         // optional flag
    Flag<"--center", Point, "-o">               center{1, 2};  // custom   flag, custom alias
    Flag<"--sep", char>                         separator;     // char     flag
    Flag<"-e", int>                             error_one;
    Flag<"-f", char>                            error_two;
  };

  const char* argv[] = {
      "--path",    "/usr",                        // string
      "--path",    "/home",                       // override
      "--port",    "8080",                        // int
      "-p",        "8090",                        // override by alias
      "--verbose",                                // bool
      "--fruits",  "orange",                      // vector
      "--fruits",  "banana",                      //
      "--drink",   "wine",                        // optional
      "--center",  "1 2",                         // custom type
      "--sep",     ".",                           // char
      "one",                                      // arg
      "--two",                                    // unknown flag
      "-e",        "nan",                         // bad value
      "-f",                                       // missing value
      "-e",        "ana",                         // bad value again
      "-f",        "xx",                          // bad value again
      "--",        "a",      "-b", "--port", "0"  // catchall
  };

  const auto [flags, args, errors] = TestFlags::Parse(argv);

  ASSERT_THAT(flags.path, StrEq("/home"));
  ASSERT_THAT(flags.port, Eq(8090));
  ASSERT_TRUE(flags.verbose);
  ASSERT_THAT(flags.fruits.value, ElementsAre("orange", "banana"));
  ASSERT_THAT(flags.drink.value, Optional(StrEq("wine")));
  ASSERT_THAT(flags.center->x, Eq(1));
  ASSERT_THAT(flags.center->y, Eq(2));
  ASSERT_THAT(flags.separator, Eq('.'));

  ASSERT_THAT(args, ElementsAre("one", "a", "-b", "--port", "0"));

  using Error = FlagInfo::Error;
  ASSERT_THAT(errors, ElementsAre(Error{.pos = 20, .arg = "--two"},               //
                                  Error{.pos = 21, .arg = "-e", .val = "nan"},    //
                                  Error{.pos = 23, .arg = "-f", .val = nullptr},  //
                                  Error{.pos = 24, .arg = "-e", .val = "ana"},    //
                                  Error{.pos = 26, .arg = "-f", .val = "xx"}));

  ASSERT_TRUE(errors);

  std::stringstream stream;
  stream << errors;
  EXPECT_THAT(stream.str(), StrEq(R"(
Unknown flag `--two` at index 20
Invalid value "nan" for flag `-e` at index 21
Missing value for flag `-f` at index 23
Invalid value "ana" for flag `-e` at index 24
Invalid value "xx" for flag `-f` at index 26
)"));

  TestFlags for_introspection;

  std::vector<std::pair<std::string_view, std::string_view>> names;
  for (const auto* flag_info : for_introspection.FlagInfos()) {
    names.push_back(std::make_pair(flag_info->name, flag_info->alias));
  }
  using testing::Pair;
  EXPECT_THAT(names, ElementsAre(Pair("--path", "--path"),        //
                                 Pair("--port", "-p"),            //
                                 Pair("--verbose", "--verbose"),  //
                                 Pair("--fruits", "--fruits"),    //
                                 Pair("--drink", "--drink"),      //
                                 Pair("--center", "-o"),          //
                                 Pair("--sep", "--sep"),          //
                                 Pair("-e", "-e"),                //
                                 Pair("-f", "-f")));
}

TEST(FlagsTest, AdvancedDemo) {
  struct SharedFlags : Flags<SharedFlags> {
    Flag<"-v", bool> verbose;
  };

  struct TestFlags : Flags<TestFlags> {
    Flag<"--port", int> port;
  };

  const char* argv[] = {
      "-v",              //
      "--port", "8080",  //
      "--unknown",       //
      "value"            //
  };

  // Shared flags can be parsed first, then TestFlags.
  {
    auto [shared_flags, args, errors] = SharedFlags::Parse(argv, false);
    ASSERT_THAT(errors, IsEmpty());
    ASSERT_TRUE(shared_flags.verbose.value);

    auto flags = TestFlags::Parse(args, errors);
    ASSERT_THAT(errors, ElementsAre(FlagInfo::Error{.pos = 2, .arg = "--unknown"}));
    ASSERT_THAT(flags.port, Eq(8080));

    ASSERT_THAT(args, ElementsAre("value"));
  }
  // Or the other way around.
  {
    auto [flags, args, errors] = TestFlags::Parse(argv, false);
    ASSERT_THAT(errors, IsEmpty());
    ASSERT_THAT(flags.port, Eq(8080));

    auto shared_flags = SharedFlags::Parse(args, errors);
    ASSERT_THAT(errors, ElementsAre(FlagInfo::Error{.pos = 1, .arg = "--unknown"}));
    ASSERT_TRUE(shared_flags.verbose.value);

    ASSERT_THAT(args, ElementsAre("value"));
  }
}

TEST(FlagsTest, IncompleteTerminalArgs) {
  struct TestFlags : Flags<TestFlags> {
    Flag<"-n", int>  count;
    Flag<"-v", bool> verbose;
  };

  {
    const char* argv[]         = {"-n"};
    auto [flags, args, errors] = TestFlags::Parse(argv);
    ASSERT_THAT(args, IsEmpty());
    ASSERT_THAT(errors, ElementsAre(FlagInfo::Error{.pos = 0, .arg = "-n", .val = nullptr}));
  }
  {
    const char* argv[]         = {"-v"};
    auto [flags, args, errors] = TestFlags::Parse(argv);
    ASSERT_THAT(args, IsEmpty());
    ASSERT_THAT(errors, IsEmpty());
  }
}

}  // namespace
}  // namespace xdk
