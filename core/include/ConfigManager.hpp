#pragma once
#include "MasterConfigParser.hpp"


class ConfigManager
{
public:
    static constexpr bool COLORS = true;

    std::vector<Profile> m_profiles;

    const fs::path HOME = core::os::userHomePath();  // this throws exception on fail, nice thing
    fs::path config_d = HOME/core::os::get_config_d()/"dotty";
    fs::path data_d = HOME/".local/share/dotty";

    const char* const master_src = ".dotty.toml";
    const char* const config_src = "config";
    const char* const data_cfgref = ".dotty.d";
    Profile m_current_profile = Profile{Profile::NOT, "", false, false};

    std::vector<SrcDest> files_to_copy = {};
    std::vector<SrcDest> files_to_link = {};
    std::vector<SrcDest> dirs_to_copy = {};
    std::vector<SrcDest> dirs_to_link = {};
    std::vector<SrcDest> sudo_files_to_copy = {};
    std::vector<SrcDest> sudo_files_to_link = {};
    std::vector<SrcDest> sudo_dirs_to_copy = {};
    std::vector<SrcDest> sudo_dirs_to_link = {};

    enum class Res : uint8_t {
        OK=0,
        ERR=1,
        PathDoesNotExist=2,
        FileCouldNotBeOpened=3,
        DirectoryCouldNotBeCreated=4,
        ProfileDoesNotExist=5,
        ProfileAlreadyExists=6,
        ProfileAlreadySet=7,
    };


    Report validateProfileName(const std::string& name);
    Report validateRepoName(const std::string& repo);
    bool noProfilesExist();
    bool profileExists(const strview profile_name);
    Profile* getProfileByName(const strview prof_name);
    std::string activeProf();
    Report newProfile(const std::string& name, const std::string& github_name,
        const std::string& repo_name, bool is_public,
        bool is_external, const char* const initial_commit_message
    );
    Report deleteProfile(const strview profile_name);
    Report setActiveProfile(const strview name);
    Report listProfiles(bool name, bool repo, bool url, bool gh);
    Report cleanConfigs(bool config, bool storage);
    bool detectPreinitConfig();
    Report reloadConfig();
    void load(bool first_load);
    std::array<std::vector<SrcDest>, 8> systemToRepo();
    void repoToSystem();
};

extern ConfigManager dotty;
