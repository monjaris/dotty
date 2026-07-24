#pragma once
#include "ConfigManager.hpp"

namespace CLI { class App; }


struct CmdLine
{
    struct Impl;
    Impl* impl = nullptr;

    CmdLine (int argc, char** argv);
    ~CmdLine ();

    // arg-fmt: "master" or "master,storage"
    // int32 do_clean(const strview options);
    int32 do_init();
    int32 do_update();
    int32 do_push(const char* commit_message);
    int32 do_pull();
    int32 do_config(strview options, const strview editor);
    int32 do_profile_(strview options);
        int32 do_p_list(const strview options);
        int32 do_p_new(const std::string& name, const std::string& repo, bool pub, const std::string& com_msg);
        int32 do_p_delete(const std::string& options);
        int32 do_p_switch(const std::string& profile_name);

    CLI::App* newSubCmd(
        CLI::App* parent, inilist<const char*> names, const std::function<int32()>& fn, const char* desc,
        bool profile_agnostic, const int32 (&opt_min_max)[2], const int32 (&sc_min_max)[2]
    );

    int32 setup();
    int32 run();
};
