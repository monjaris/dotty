#pragma once
#include "DotlangParser.hpp"


// methods with 'r..' prefix are config readers,
// whereas 'w..' ones are config writers.
struct MasterConfigParser {
    struct Pimpl_Toml;
    Pimpl_Toml* m_toml = nullptr;
    std::map<std::string, std::string> vars;
    std::vector<Profile> profiles;

    // Property symbol table
    static COMPTIME_STR P_ACTIVE_PROF   = "active-profile";
    static COMPTIME_STR P_PROFILES  = "profile";
        static COMPTIME_STR PP_NAME     = "name";  // str
        static COMPTIME_STR PP_REPO_URL = "url";  // str
        static COMPTIME_STR PP_REPO_PUB = "public";  // bool
        static COMPTIME_STR PP_EXTERNAL = "external";  // bool
    static COMPTIME_STR P_CFG_EDITOR   = "config-editor";


    MasterConfigParser ();
    ~MasterConfigParser ();

    Report rParse(const fs::path& path);
    Report rEval();
    Report rValidateConfig();

    Report wActivateProfile(const strview name);
    Report wSetDefaultEditor(const strview editor);
    Report wAddProfile(const Profile& prof);
    Report wRemoveProfile(const strview name);
    Report wSaveConfig(const fs::path& path);
};

