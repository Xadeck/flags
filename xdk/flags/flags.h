#ifndef XDK_FLAGS_FLAGS_H_
#define XDK_FLAGS_FLAGS_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

namespace xdk {

struct FlagInfo {
  // 1. Unknown flag
  //    `pos`: the index of argument that is not a flag
  //    `arg`: points to `argv[pos]` and starts by `-`
  //    `val`: points to `kUnknown`.
  // 2. Invalid flag value
  //    `pos`: the index of argument that is the flag
  //    `arg`: points to `argv[pos]` and is the name of the flag
  //    `val`: points to `argv[pos+1]` and is the string not valid as a value
  // 3. Missing flag value
  //    `pos`: as in previous case
  //    `arg`: as in previous case
  //    `val`: points to `nullptr`
  struct Error {
    static inline const char kUnknown[] = "unknown";

    int         pos = 0;
    const char* arg = nullptr;  // non-null for errors returnes by `Flags::Parse`.
    const char* val = kUnknown;

    friend bool operator==(const FlagInfo::Error&, const FlagInfo::Error&) = default;
  };

  struct Errors : public std::vector<Error> {
    operator bool() const {  // NOLINT
      return !empty();
    }

    friend std::ostream& operator<<(std::ostream& os, const Errors& errors) {
      os << '\n';
      for (const auto& error : errors) {
        if (error.val == Error::kUnknown) {
          os << "Unknown flag `" << error.arg << '`';
        } else if (error.val == nullptr) {
          os << "Missing value for flag `" << error.arg << '`';
        } else {
          os << "Invalid value " << std::quoted(error.val) << " for flag `" << error.arg << '`';
        }
        os << " at index " << error.pos << '\n';
      }
      return os;
    }
  };

  // For introspection
  std::string_view      name;
  const std::type_info* type = nullptr;
  std::string_view      alias;

  template <size_t N>
  struct String {
    constexpr String(const char (&str)[N]) {  //  NOLINT cppcheck-suppress noExplicitConstructor
      std::copy_n(str, N, array.data());
    }
    std::array<char, N> array;

    [[nodiscard]] constexpr bool IsValid() const {
      if (N == 1 || array[0] != '-') return false;  // empty or not starting with -
      if (N == 3 && array[1] == '-') return false;  // equal to --
      return true;
    }
  };

  enum class ParseStatus { kNoneParsed, kOneParsed, kTwoParsed, kParseMissing, kParseFailure };

  // For parsing
  std::size_t                                          size = 0;
  std::function<ParseStatus(const char*, const char*)> parse;
};

template <typename T>
bool ParseValue(const char* arg, T& value) {
  std::istringstream stream(arg);
  stream >> value;
  return !stream.fail();
}

template <>
inline bool ParseValue(const char* arg, char& value) {
  value = arg[0];
  return arg[1] == 0;
}

template <typename T>
bool ParseValue(const char* arg, std::vector<T>& value) {
  value.emplace_back();
  return ParseValue(arg, value.back());
}

template <typename T>
bool ParseValue(const char* arg, std::optional<T>& value) {
  value.emplace();
  return ParseValue(arg, *value);
}

template <FlagInfo::String L, typename T, FlagInfo::String A = L>
class Flag final : private FlagInfo {
  static_assert(L.IsValid(), "must start with - and be different from --");
  static_assert(A.IsValid(), "must start with - and be different from --");

 public:
  template <typename... Args>
  explicit Flag(Args&&... args) : value(std::forward<Args>(args)...) {
    size  = sizeof(*this);
    parse = [this](const char* name, const char* value) {
      using enum ParseStatus;
      if (kL != name && kA != name) return kNoneParsed;
      if constexpr (std::is_same<T, bool>::value) {
        this->value = true;
        return kOneParsed;
      }
      if (value == nullptr || value[0] == '-') return kParseMissing;
      return ParseValue(value, this->value) ? kTwoParsed : kParseFailure;
    };
    name  = kL;
    type  = &typeid(T);
    alias = kA;
  }

  operator const T&() const {  // NOLINT
    return value;
  }
  const T* operator->() const {
    return &value;
  }

  T value;

 private:
  static constexpr std::string_view kL{L.array.data(), L.array.size() - 1};
  static constexpr std::string_view kA{A.array.data(), A.array.size() - 1};
};

template <typename F>
class Flags {
 public:
  template <FlagInfo::String L, typename T, FlagInfo::String A = L>
  using Flag = ::xdk::Flag<L, T, A>;

  static auto Parse(int argc, char** argv, bool unknown_are_errors = true) {
    return Parse(argc, const_cast<const char**>(argv), unknown_are_errors);
  }

  template <size_t N>
  static auto Parse(const char* (&argv)[N], bool unknown_are_errors = true) {
    return Parse(N, argv, unknown_are_errors);
  }

  static auto Parse(int argc, const char** argv, bool unknown_are_errors = true) {
    F                        f;
    std::vector<const char*> args;
    FlagInfo::Errors         errs;
    Parse(argc, argv, f, args, errs, unknown_are_errors);
    return std::make_tuple(std::move(f), std::move(args), std::move(errs));
  }

  static auto Parse(std::vector<const char*>& old_args, FlagInfo::Errors& errs) {
    F                        f;
    std::vector<const char*> new_args;
    Parse(static_cast<int>(old_args.size()), old_args.data(), f, new_args, errs);
    new_args.swap(old_args);
    return f;
  }

  [[nodiscard]] std::vector<const FlagInfo*> FlagInfos() const {
    const char* f_begin = reinterpret_cast<const char*>(this);
    const char* f_end   = reinterpret_cast<const char*>(this) + sizeof(F);

    std::vector<const FlagInfo*> infos;
    for (const char* pf = f_begin; pf < f_end; pf += infos.back()->size) {
      infos.push_back(reinterpret_cast<const FlagInfo*>(pf));
    }
    return infos;
  }

 private:
  static void Parse(int argc, const char** argv, F& f, std::vector<const char*>& args,
                    FlagInfo::Errors& errs, bool unknown_are_errors = true) {
    static_assert(sizeof(Flags<F>) == 1);
    static_assert(sizeof(F) > 1);

    static constexpr std::string_view kDashDash = "--";

    const char* f_begin = reinterpret_cast<const char*>(&f);
    const char* f_end   = reinterpret_cast<const char*>(&f) + sizeof(F);
    int         pos     = 0;
    while (pos < argc) {
      const char*                    arg    = argv[pos];
      const char*                    val    = pos + 1 < argc ? argv[pos + 1] : nullptr;
      int                            parsed = 0;
      std::optional<FlagInfo::Error> error  = std::nullopt;
      if (kDashDash == arg) break;
      for (const char* pf = f_begin; !parsed && pf < f_end;) {
        const auto* info = reinterpret_cast<const FlagInfo*>(pf);
        switch (info->parse(arg, val)) {
          using enum FlagInfo::ParseStatus;
          case kNoneParsed:   break;
          case kOneParsed:    parsed = 1; break;
          case kTwoParsed:    parsed = 2; break;
          case kParseMissing: parsed = 1, error = {.pos = pos, .arg = arg, .val = nullptr}; break;
          case kParseFailure: parsed = 2, error = {.pos = pos, .arg = arg, .val = val}; break;
        }
        pf += info->size;
      }
      if (error.has_value()) errs.push_back(*error);
      if (parsed == 0) {
        if (arg[0] == '-' && unknown_are_errors) {
          errs.push_back({.pos = pos, .arg = arg});
        } else {
          args.push_back(arg);
        }
      }
      pos += std::max(1, parsed);
    }
    while (++pos < argc) args.push_back(argv[pos]);
  }
};

}  // namespace xdk

#endif  // XDK_FLAGS_FLAGS_H_
