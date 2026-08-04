#pragma once
#include <wordexp.h>
#include "utils.hpp"
NAMESPACE_START(core::os)


// all environment variables of the current process
inline char** get_environ() {
    return ::environ;  // global unistd variable
}


struct CmdTokens {
    wordexp_t m_wordexp;
    const char* m_line;
    bool m_parsed = false;

public:
    CmdTokens (const char* line): m_line(line) {}
    ~CmdTokens () { if (m_parsed) ::wordfree(&m_wordexp); }

    const char* line() {
        return m_line;
    }

    const char* const* const words() {
        return m_wordexp.we_wordv;
    }

    usize count() {
        return m_wordexp.we_wordc;
    }

    bool parse(int flags = 0) {
        if (::wordexp(m_line, &m_wordexp, flags) != 0) return false;
        m_parsed = true;

        if (m_wordexp.we_wordc == 0) return false;

        return true;
    }

    bool run() {
        return ::execvp(m_wordexp.we_wordv[0], m_wordexp.we_wordv) != -1;
    }
};


constexpr uint32 EXEC_MAX_ARGS = 128;

// replaces current process with given program and it's arguments
inline bool exec(const char* program, inilist<const char*> args) {
    char* argv[EXEC_MAX_ARGS];
    argv[0] = const_cast<char*>(program);
    uint32 iw = 1;
    for (auto& arg : args) {
        if (iw >= EXEC_MAX_ARGS-1) break;
        if (arg == nullptr) break;
        argv[iw] = const_cast<char*>(arg);
        ++iw;
    }
    argv[iw] = nullptr;
    return ::execvp(program, argv) != -1;
}

// unlike core::os::exec(), takes a line instead of tokens
inline bool exec_line(const char* cmd_line) {
    CmdTokens cmd_tokens(cmd_line);
    if (cmd_tokens.parse() == false) return false;
    return cmd_tokens.run();
}

// implements returning prog(prog,{argv,nullptr}) to spawn_child() overloads
static pid_t impl_spawn_child(const char* prog, char* argv[]) {
    pid_t pid;
    uint32 status = posix_spawnp(
        &pid, prog, nullptr, nullptr,
        argv,
        get_environ()
    );
    return (status==0)? pid : -1;
}

// spawn process(posix compliant), returns -1 on failure
inline pid_t spawn_child(const char* prog, const char* const* args) {
    char* argv[EXEC_MAX_ARGS];
    argv[0] = const_cast<char*>(prog);

    usize ir = 1;  // read index of args, [0] is prog
    for (usize iw=0;  iw < EXEC_MAX_ARGS-1-1;  ++iw) {
        if (args[iw] == nullptr || (ir >= EXEC_MAX_ARGS-1)) break;
        argv[ir++] = const_cast<char*>(args[iw]);
    }
    argv[ir] = nullptr;
    return impl_spawn_child(prog, argv);
}

// overload with initializer_list type
inline pid_t spawn_child(const char* prog, inilist<const char*> arg_list) {
    char* argv[EXEC_MAX_ARGS];
    argv[0] = const_cast<char*>(prog);

    usize ir = 1;  // read index of args, [0] is prog
    for (auto& arg  : arg_list) {
        if (arg == nullptr || (ir >= EXEC_MAX_ARGS-1)) break;
        argv[ir++] = const_cast<char*>(arg);
    }
    argv[ir] = nullptr;
    return impl_spawn_child(prog, argv);
}


// wait for process to finish by passing the id of that process
inline int32 wait_proc(pid_t pid) {
    int32 status;
    if (waitpid(pid, &status, 0) == -1) return -1;
    if (!WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

inline bool in_path(const char* name) {
    pid_t pid = ::fork();
    if (pid == -1) return false;
    // only child proc runs this branch bcz fork returns two values in both procs
    if (pid == 0) {
        int devnull = ::open("/dev/null", O_WRONLY);
        ::dup2(devnull, STDOUT_FILENO);
        ::dup2(devnull, STDERR_FILENO);
        ::close(devnull);
        exec("which", {name});
        ::exit(1);  // execlp failed and program reaches here so exit child proc
    }
    return wait_proc(pid) == 0;
}

// load $HOME to static constant once and return it
inline const char* userHomePath() {
    static const char* home_path_cache = nullptr;
    if (home_path_cache != nullptr) return home_path_cache;

    const char* home_env = nullptr;
    #if defined(DOTTY_GENERIC_UNIX)
        home_env = ::getenv("HOME");
    #elif defined(_WIN32)
        home_env = ::getenv("USERPROFILE");
    #endif

    if (home_env == nullptr) {
        throw std::runtime_error("HOME environment variable is not set!");
    } else {
        return (home_path_cache = home_env);
    }
}


// get configuration directory based on the OS
inline fs::path get_config_d() {
    #define NO_PATH "////////////////////////"
    static fs::path os_config_dir = NO_PATH;
    // operate once and keep in static storage
    if (os_config_dir == NO_PATH) {
        #if defined(DOTTY_FOSS_UNIX)
            const char* env = ::getenv("XDG_CONFIG_HOME");
            if (env) return os_config_dir = env;
            else return os_config_dir = cat_path(userHomePath(), ".config");
        #elif defined(__APPLE__)
            return os_config_dir = cat_path(userHomePath(), "Library/Preferences");
        #elif defined(_WIN32)
            return os_config_dir = cat_path(userHomePath(), "AppData/Roaming");
        #else
            return os_config_dir = cat_path(userHomePath(), ".config");
        #endif
    }
    else return os_config_dir;
}


// get system editor with nice fallbacks
inline const char* get_txt_editor() {
    static const char* env_visual = ::getenv("VISUAL");
    static const char* env_editor = ::getenv("EDITOR");
    static const char* text_editor = nullptr;

    if (text_editor == nullptr) {
        if (env_editor)  return (text_editor = env_editor);
        if (env_visual)  return (text_editor = env_visual);
        else if (!::system("which nano >" NULLDEV)) return (text_editor = "nano");
        else if (!::system("which vi >" NULLDEV))   return (text_editor = "vi");
        else return nullptr;
    }
    else {
        return text_editor;
    }
}


NAMESPACE_END(core::os)
