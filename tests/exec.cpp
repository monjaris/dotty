#include "core.hpp"

using namespace core;

int main() {
    const char* home = ::getenv("HOME");

    if (home == nullptr) {
        os::exec("echo", {"how", "?"});
    }

    // replace this test with new process
    os::exec("dolphin", {"--split"});
    // wont run because previous line already replaced the process memory
    os::exec_line("dolphin --split");
}
