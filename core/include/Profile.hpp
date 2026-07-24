#pragma once
#include "core.hpp"

struct SrcDest { fs::path src, dest; };

struct Profile {
    static COMPTIME_STR NOT = "[NIL-PROFILE]";

    std::string name;
    std::string repo_url;
    bool is_pub = false;  // repo visibility is public or not
    bool is_ext = false;  // the profile repo is owned by user or not

    Profile(
        const std::string& name,
        const std::string& repo_url,
        const bool is_public,
        const bool is_extern
    );

};
