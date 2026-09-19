#pragma once
#include "MasterConfigParser.hpp"


class ConfigManager
{
public:
    static constexpr bool COLORS = true;

    std::vector<Profile> m_profiles;

    // Resolved at construction; empty HOME is tolerated and reported later.
    const fs::path HOME;
    fs::path config_d;
    fs::path data_d;

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
    std::vector<std::string> exec_commands = {};

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

    ConfigManager();

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
    // Returns Report; never throws. first_load creates missing skeleton files.
    Report load(bool first_load);
    // Apply system → repo mappings. Returns Report (never throws).
    Report systemToRepo();
    // Apply repo → system mappings. Returns Report (never throws).
    Report repoToSystem();
    // Run collected @exec commands via posix_spawn (no shell).
    Report runExecCommands();
};

extern ConfigManager dotty;
