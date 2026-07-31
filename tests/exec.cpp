#include "core.hpp"

int main() {
    const char* home = ::getenv("HOME");
    if (home == nullptr) {
        cm::os::exec("echo", {"how", "?"});
    }
    // replace current process with new process
    cm::os::exec("$VISUAL", {home});
}
