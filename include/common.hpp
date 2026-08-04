#pragma once
// general
#include <iostream>
#include <filesystem>
#include <concepts>
#include <thread>
#include <utility>
#include <ranges>
// stl classes
#include <vector>
#include <map>
#include <string_view>
#include <functional>
#include <expected>
#include <fstream>
#include <sstream>
// legacy
#include <cstdint>
#include <cstdio>
#include <cstring>
// posix
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
// 3rd party
// #include "toml++/toml.hpp"
// #include "CLI/CLI.hpp"
#include "dotline/include/dotline.hpp"


#define NAMESPACE_START(_name) namespace _name {
#define NAMESPACE_END(_name) }
#define COMPTIME_STR constexpr const char* const


#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__))
#   define DOTTY_FOSS_BSD
#endif

#if (defined(__linux__) || defined(DOTTY_BSD))
#   define DOTTY_FOSS_UNIX
#endif

#if (defined(__APPLE__) || defined(DOTTY_BSD))
#   define DOTTY_GENERIC_BSD
#endif

#if (defined(DOTTY_FOSS_UNIX) || defined(__APPLE__))
#   define DOTTY_GENERIC_UNIX
#endif


#if defined(__linux__) || defined(__APPLE__)
#       define OS_NEWLN '\n'
#       define NULLDEV   "/dev/null 2>&1"
#elif defined(_WIN32)
#   define OS_NEWLN '\r'
#endif

inline std::vector<std::string> unimplemented;
// bad yes, but i need highlight for better notice(GCC/Clang specific! who cares MSVC though)
#define $IMPLEMENT(_feature) (unimplemented.push_back(std::string(std::string(__func__)+": ")+_feature), (void)"")
#define $PRINT_UNIMPLEMENTED_FEATURES() \
    do{if(unimplemented.size())std::cerr<<"\033[33mUNIMPLEMENTED:\n"; \
        for(auto f:unimplemented)std::cerr<<f<<"\n" \
    ;}while(0)

using namespace std::string_literals;
namespace fs = std::filesystem;
namespace core {};

template<typename T> concept arithmetic = std::is_arithmetic_v<T>;

using int32 = int32_t;
using uint32 = uint32_t;
using usize = size_t;
using int64 = int64_t;
using strview = std::string_view;
template<class T> using inilist = std::initializer_list<T>;
template<class T, class U=std::default_delete<T>> using Uptr = std::unique_ptr<T, U>;
