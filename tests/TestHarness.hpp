#ifndef BOOTEABLE_TESTS_TESTHARNESS_HPP
#define BOOTEABLE_TESTS_TESTHARNESS_HPP

#include <sstream>
#include <string>
#include <type_traits>

namespace booteable::test {

extern int failures;

void report_failure(const char* file, int line, const std::string& message);

template <typename T>
std::string describe(const T& value) {
    using Decayed = std::decay_t<T>;
    if constexpr (std::is_same_v<Decayed, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<Decayed, const char*> ||
                         std::is_same_v<Decayed, char*>) {
        return value != nullptr ? std::string(value) : std::string("(null)");
    } else if constexpr (std::is_same_v<Decayed, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<Decayed>) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_integral_v<Decayed>) {
        return std::to_string(value);
    } else if constexpr (std::is_floating_point_v<Decayed>) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else {
        return "<valor>";
    }
}

} // namespace booteable::test

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            ::booteable::test::report_failure(                               \
                __FILE__, __LINE__, "CHECK falló: " #condition);             \
        }                                                                    \
    } while (false)

#define CHECK_EQ(actual, expected)                                           \
    do {                                                                     \
        const auto& booteable_actual = (actual);                             \
        const auto& booteable_expected = (expected);                         \
        if (!(booteable_actual == booteable_expected)) {                      \
            ::booteable::test::report_failure(                               \
                __FILE__, __LINE__,                                          \
                std::string("CHECK_EQ falló: " #actual " != " #expected      \
                            " (") +                                          \
                    ::booteable::test::describe(booteable_actual) + " vs " + \
                    ::booteable::test::describe(booteable_expected) + ")");  \
        }                                                                    \
    } while (false)

#endif // BOOTEABLE_TESTS_TESTHARNESS_HPP
