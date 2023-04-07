//
// Created by Maksym Pasichnyk on 04.04.2023.
//

#pragma once

#include <version>
#include <concepts>
#ifdef __cpp_lib_coroutine
    #include <coroutine>
#endif
#include <any>
#include <bitset>
#include <chrono>
#include <compare>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <initializer_list>
#include <optional>
#ifdef __cpp_lib_source_location
    #include <source_location>
#endif
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <variant>
#include <memory>
#ifdef __cpp_lib_memory_resource
    #include <memory_resource>
#endif
#include <new>
#include <scoped_allocator>
#include <cfloat>
#include <cinttypes>
#include <climits>
#include <cstdint>
#include <limits>
#include <cassert>
#include <cerrno>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <cctype>
#include <charconv>
#include <cstring>
#include <cuchar>
#include <cwchar>
#include <cwctype>
#ifdef __cpp_lib_format
    #include <format>
#endif
#include <string>
#include <string_view>
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <span>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iterator>
#ifdef __cpp_lib_ranges
    #include <ranges>
#endif
#include <algorithm>
#include <execution>
#include <bit>
#include <cfenv>
#include <cmath>
#include <complex>
#ifdef __cpp_lib_math_constants
    #include <numbers>
#endif
#include <numeric>
#include <random>
#include <ratio>
#include <valarray>
#include <clocale>
#include <codecvt>
#include <locale>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <ostream>
#ifdef __cpp_lib_spanstream
    #include <spanstream>
#endif
#include <sstream>
#include <streambuf>
#ifdef __cpp_lib_syncstream
    #include <syncstream>
#endif
#include <filesystem>
#include <regex>
#include <atomic>
#ifdef __cpp_lib_barrier
    #include <barrier>
#endif
#include <condition_variable>
#include <future>
#ifdef __cpp_lib_latch
    #include <latch>
#endif
#include <mutex>
#ifdef __cpp_lib_semaphore
    #include <semaphore>
#endif
#include <shared_mutex>
//#include <stop_token>
#include <thread>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = size_t;
using isize = ptrdiff_t;