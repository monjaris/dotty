#pragma once
#pragma GCC diagnostic ignored "-Wignored-attributes"
#include "common.hpp"
#include "utils.hpp"
#include "os.hpp"

#ifndef DEBUG_ON
#   define DEBUG_ON (1)
#endif

#ifndef PRINT_ON
#   define PRINT_ON (1)
#endif


NAMESPACE_START(core)

struct Report;


inline void initialize() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
}

// print to stdout via std::ostream
template <bool _flush=true, class... Args>
inline void print(Args&&... args) {
#if !PRINT_ON
        return;
#endif
    (std::cout << ... << std::forward<Args>(args));
    if(_flush) std::flush(std::cout);
}


template <class... Args>
inline void debug(Args... args) {
#if !DEBUG_ON
    return;
#endif
    const strview pre = "\033[2m[Debug]\033[0m ";
    const strview post = "\n";
    core::print(pre, std::forward<Args>(args)..., post);
}

template <class... Args>
inline void perror(std::format_string<Args...>, Args... args) {
    $IMPLEMENT("impl this function and make sure its the main function for printing errors!");
}


// uses std::cin or dotl::read_string()
template <bool line_noise=false, class T>
inline T& prompt(const char* prompt, T& lval) {
    // Read input relevantly
    if constexpr (std::is_same_v<char, T> || std::is_convertible_v<char, T>) {
        if (!line_noise) {
            lval = dotl::prompt(prompt).read_string();
            core::print("\n");
        } else {
            core::print(prompt);
            std::cin >> lval;
        }
    }
    else
    {
        if constexpr (!line_noise) {
            dotl::prompt(prompt);
            lval = dotl::read_string().string();
            core::print("\n");
        }
        else {
            core::print(prompt);
            std::getline(std::cin, lval);
        }
    }

    return lval;
}

// for arithmetic values
template <class Number, class StringType>
requires std::is_arithmetic_v<Number> && std::same_as<StringType, const char*>
inline void prompt_number(const StringType prompt, Number& number, bool std_line_reader = false) {
    if (std_line_reader) {
        core::print(prompt);
        std::cin >> number;
    }
    else {
        dotl::prompt(prompt);
        dotl::ParseRes res = dotl::read_string().parse<Number>();
        if (!res.error()) {
            number = res.value();
        } else {
            return;
        }
    }
}


// utility for YES/NO questions
inline bool ask_confirm(const std::string& message, bool default_yes = true) {
    const char* post = default_yes?(" (Y/n): "):(" (y/N): ");
    std::string ask_msg = std::string(message).append(post);

    std::string inps = dotl::prompt(ask_msg).read_string().rmFirst('\n');
    if (inps.empty()) return(default_yes);

    if (inps.empty()) return default_yes;
    if (::tolower((unsigned)inps[0]) == 'y') return true;
    return(false);
}


// terminate the application
template <int _errc=1, class... Args>
[[noreturn]] inline void terminate(Args&&... args) {
    (std::cerr << ... << std::forward<Args>(args)) << std::endl;
    ::exit(_errc);
}



inline bool is_even(int32 x) {
    return (x & 1) == 0;
}

template <class T>
bool is_any_of(const T val, inilist<T> list) {
    for (auto item : list) {
        if (val == item) {
            return true;
        }
    }
    return false;
}

inline std::string concats(const char* base, const char* append) {
    return std::string(base).append(append);
}


// get pair of strings, first being left of the first '.' and second being the rest
inline std::pair<std::string, std::string> obj_prop(std::string str, char sep='.') {
    std::string rest;
    for (uint32 i=0;  i < str.size();  ++i) {
        if (str[i] == sep) {
            rest = str.substr(i);
            str = str.substr(0, i);
            return {std::move(str), std::move(rest)};
        }
    }
    return {std::move(str), ""};
}

template <class Container, class Underlying>
inline bool contains(const Container& cont, const Underlying& element) {
    return std::find(cont.begin(), cont.end(), element) != cont.end();
}


inline bool str_has_any_of(const std::string& str, inilist<char> chars) {
    for (char c : chars) {
        if (str.contains(c)) return true;
    }
    return false;
}

// takes @str writes to @out only if @prefix is @str's prefix, else returns false
inline bool prefix_strip(const std::string& str, const strview prefix, std::string* out) {
    if (str.substr(0, prefix.size()) != prefix) return false;
    if (out) *out = str.substr(prefix.size());
    return true;
}


// create a new file, return false if unsuccessful
inline bool new_file(const fs::path& path) {
    if (fs::exists(path)) return false;
    std::ofstream file(path);
    return file.good();
}

// creates directory if doesnt exist, else no-op
inline void ensure_directories(const fs::path& dir_path) {
    fs::create_directories(dir_path);
}

// copy while directory recursively without worrying about flags to pass
inline void copy_directory(const fs::path& src_d, const fs::path& dest_d, bool cp_if_src_is_newer=false) {
    fs::copy(src_d, dest_d,
        fs::copy_options::recursive | (cp_if_src_is_newer?
        fs::copy_options::update_existing : fs::copy_options::overwrite_existing
    ));
}


// parse file path by converting tilde('~') to $HOME variable
constexpr inline fs::path parsePathTilde(std::string path) {
    if (!(path[0] == '~')) return path;
    path.erase(0, 1);
    const char* const user_home = ::core::os::userHomePath();
    path.insert(0, user_home);
    return path;
}

inline bool is_file_empty(fs::path file_path) {
    return fs::file_size(file_path) == 0uz;
}

inline void empty_file(fs::path file_path) {
    std::ofstream ef(file_path);
}

// remove all files/subfolders inside a directory but not itself
// return true if remove results correctly(depends on `std::error_code` && `fs::remove_all`)
inline bool remove_directory_contents(
    fs::path directory, inilist<const char*> exclude={}
) {
    std::error_code remove_result;

    for (auto& item : fs::directory_iterator(directory)) {
        if (!core::contains(exclude, item.path().filename().c_str())) {
            fs::remove_all(item, remove_result);
        }
    }
    core::debug(remove_result.message());
    return remove_result.value() == 0;
}

// TODO: Add explanation to this function
inline std::pair<int32, int32> remove_dir_contents_recursive(
    fs::path directory, inilist<const char*> exclude={}
) {
    uint32 total_c = 0;
    uint32 removed_c = 0;

    for (auto& item : fs::directory_iterator(directory)) {
        total_c += fs::exists(item) ? std::distance(
            fs::recursive_directory_iterator(item),
            fs::recursive_directory_iterator{}
        ) + 1 : 1;
        if (core::contains(exclude, item.path().filename().c_str()))
            continue;
        std::error_code ec;
        removed_c += fs::remove_all(item, ec);
    }

    return {removed_c, total_c};
}


COMPTIME_STR PPRINTER = "bat";
// pretty-print file, calls 'bat' (cat alternative)
inline bool pprint_file(const char* const fpath) {
    pid_t pid = os::spawn_child(PPRINTER, {fpath});
    if (pid == -1) return false;
    return os::wait_proc(pid) != -1;
}
// overload for const path reference
inline bool pprint_file(const fs::path& fpath) {
    return pprint_file(fpath.c_str());
}


[[nodiscard]] inline
std::string make_repo_url(const strview github_name, const strview repo_name) {
    const std::string url = std::format("https://github.com/{}/{}", github_name, repo_name);
    return url;
}

[[nodiscard]] inline
std::string repo_from_url(const strview repo_url) {
    static constexpr const char* BAD_URL = "[BAD-URL]";
    static constexpr strview prefix = "https://github.com/";

    // starts with "https://github.com/"
    if (repo_url.size()<=prefix.size() || !repo_url.starts_with(prefix)) return BAD_URL;
    strview path = repo_url.substr(prefix.size());

    size_t first_slash = path.find('/');
    if (first_slash == strview::npos || first_slash == 0) return BAD_URL;

    strview repo_part = path.substr(first_slash + 1);
    if (repo_part.empty()) return BAD_URL;

    // remove if it has trailing slash
    size_t next_slash = repo_part.find('/');
    if (next_slash != strview::npos) repo_part = repo_part.substr(0, next_slash);

    if (repo_part.empty()) return BAD_URL;
    else return std::string(repo_part);
}

[[nodiscard]] inline
std::string gh_host_from_url(const strview repo_url) {
    static constexpr const char* BAD_URL = "[BAD-URL]";
    static constexpr strview prefix = "https://github.com/";

    if (repo_url.size() <= prefix.size() || !repo_url.starts_with(prefix)) {
        return BAD_URL;
    }
    strview path = repo_url.substr(prefix.size());

    size_t first_slash = path.find('/');
    if (first_slash == strview::npos || first_slash == 0) {
        return BAD_URL;
    }

    return std::string(path.substr(0, first_slash));
}

inline std::optional<std::string> active_github_account() {
    std::string gh_acc = {};

    Uptr<FILE, decltype(&pclose)> pipe = {
        popen("gh api user --jq '.login'", "r"),
        pclose
    };
    if (pipe == nullptr) return std::nullopt;

    char buf[256];
    while (::fgets(buf, sizeof(buf), pipe.get())) {
        gh_acc += buf;
    }

    if (gh_acc.empty()) return std::nullopt;
    return strip_nl(gh_acc);
}


// tries Cloudfare(fallbacks to Quad9) DNS at always open port 53
inline bool internet_is_connected(uint32 timeout_seconds = 2) {
    char cmd[64];
    // -z for scan only, -w for timeout seconds
    snprintf(cmd, sizeof(cmd),
        "nc -zw%u 1.1.1.1 53 2>/dev/null||nc -zw%u 9.9.9.9 53 2>/dev/null",
        timeout_seconds, timeout_seconds
    );
    return ::system(cmd) == 0;
}

// void check_internet_async(std::function<void(bool)> callback) {
    // std::thread([callback]() {
        // int result = ::system("nc -zw1 1.1.1.1 53 > /dev/null 2>&1");
        // callback(result == 0);
    // }).detach();
// }


NAMESPACE_END(core)

struct tern {
    enum class value_t { no, yes, neutr} value;
    using value_t::no;  using value_t::yes;  using value_t::neutr;

    tern (value_t ternary_value): value(ternary_value) {}

    bool boolable() const { return (value==yes) || (value==no); }
    bool b = std::invocable<std::function<void()>()>;

    operator bool() const {
        switch (value) {
            case yes: return true;
            case no: return false;
            case neutr: {
                throw std::logic_error("Can't convert neutral value to bool!");
            }
        }
        std::unreachable();
    }
};

// contains error message and an error code
struct [[nodiscard]]
core::Report {
    bool m_err = false;
    std::string m_msg = {};

    [[nodiscard]]
    static Report Good() {
        return Report {false, ""};
    }

    template <class... FmtArgs> [[nodiscard]]
    static Report Bad(std::format_string<FmtArgs...> err_msg, FmtArgs&&... err_msg_args) {
        return Report {true, std::format(err_msg, std::forward<FmtArgs>(err_msg_args)...)};
    }

    constexpr explicit inline
        operator bool () const { return m_err; }
    bool success() const { return !(bool)*this; }
    bool error() const { return (bool)*this; }
    void mute() {}  // so that nodiscard is explicitly bypassed

    void printComplains() const {
        if (!m_msg.empty()) {
            core::print(m_msg, "\n");
        }
    }

    // print `msg` and return true if `errc` is bad
    Report& printOnBad() {
        if (this->error()) this->printComplains();
        return *this;
    }

    template <class... FmtArgs>
    void addComplain(std::format_string<FmtArgs...> complain_msg, FmtArgs&&... complain_args) {
        m_err = true;
        std::string complain = "\n" + std::format(
            complain_msg, std::forward<FmtArgs>(complain_args)...
        );
        m_msg.append(complain);
    }

    void terminateOnBad() {
        if (this->error()) {
            core::terminate("Invalid action, terminating!");
        }
    }
};

using core::Report;
