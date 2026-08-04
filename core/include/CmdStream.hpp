#pragma once
#include "utils.hpp"
#include "os.hpp"
NAMESPACE_START(core)

class CmdStream
{
    std::vector<std::string> commands;
    std::string output_buf;

public:
    CmdStream () {
        commands.reserve(128);
    }

    template <class... FmtArgs>
    CmdStream& add(std::format_string<FmtArgs...> command_fmt, FmtArgs&&... fmt_args) {
        commands.push_back(std::format(command_fmt, std::forward<FmtArgs>(fmt_args)...));
        return *this;
    }

    void clear() {
        commands.clear();
    }

    // @arg break_on_fail -> inserts && if set true(if @spawn_shell is set true too)
    // @arg capture_output -> captures output via a pipe
    // @arg spawn_shell -> spawns system shell instead a raw process
    int32 run(bool break_on_fail, bool capture_output, bool spawn_shell=true) {
        output_buf.clear();

        std::string line;
        for (uint32 i = 0; i < commands.size(); ++i) {
            line += commands[i];
            if (i != commands.size() - 1) {
                if (break_on_fail && spawn_shell) {
                    line += " && ";
                }
            }
        }

        if (capture_output) {
            std::string out;
            std::unique_ptr<FILE, decltype(&pclose)> pipe = {
                popen(line.c_str(), "r"), pclose
            };
            if (pipe == nullptr) return -1;

            char buf[256];
            while (::fgets(buf, sizeof(buf), pipe.get()) != nullptr)
            {
                output_buf.append(buf);
            }
            output_buf = core::strip_nl(output_buf);

            return pclose(pipe.release());
        }

        if (spawn_shell) return ::system(line.data());
        else {
            if (commands.size() != 1) return 0;
            return core::os::exec_line(commands[0].c_str());
        }
    }

    // Output loads to internal buffer after calling .run().
    // this function, moves internal buffer when returns
    [[nodiscard]] std::string output() {
        return std::move(output_buf);
    }
};

NAMESPACE_END(core)
