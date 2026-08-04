#include "ConfigManager.hpp"
#include "CmdStream.hpp"

using CM = ConfigManager;

Report CM::validateProfileName(const std::string& name) {
    if (name == Profile::NOT) {
        return Report::Bad("Profile can't be assigned to profile sentinel('{}')", Profile::NOT);
    }
    else if (!isalpha(name[0])) {
        return Report::Bad("First character should be an alpha");
    }
    else if (std::string::npos !=
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._")
    ){
        return Report::Bad("Profile name contains illegal character");
    }

    return Report::Good();
}


Report CM::validateRepoName(const std::string& repo) {
    if (repo.empty()) {
        return Report::Bad("Repo name should not be empty");
    }

    return Report::Good();
}


bool CM::noProfilesExist() {
    return m_profiles.size() == 0;
}


bool CM::profileExists(const strview profile_name) {
    for (const Profile& prof : m_profiles) {
        if (prof.name == profile_name) {
            return true;
        }
    }
    return false;
}


Profile* CM::getProfileByName(const strview prof_name) {
    for (uint32 i=0;  i < m_profiles.size();  ++i) {
        if (m_profiles[i].name == prof_name) {
            return &m_profiles[i];
        }
    }
    // not found
    return nullptr;
}


// Get current profile as string
std::string CM::activeProf() {
    std::string profile_name = m_current_profile.name;
    return profile_name;
}



// Create a folder and register a new profile
Report CM::newProfile(
    const std::string& name, const std::string& github_name,
    const std::string& repo_name, bool is_public,
    bool is_external, const char* const initial_commit_message
){
    static COMPTIME_STR err = "Can't create profile";

    // validate profile and repo names quickly
    dotty.validateProfileName(name).printOnBad().terminateOnBad();
    dotty.validateRepoName(repo_name).printOnBad().terminateOnBad();
    if (profileExists(name)) return Report::Bad("{} '{}': Profile already exists.", err, name);
    // create profile directory and files
    if (!fs::create_directories(config_d/name)) {
        return Report::Bad(
            "Couldn't create directories, they probably already exist: '{}'",
            (config_d/name).string()
        );
    }
    if (!core::new_file(config_d/name/config_src)) return Report::Bad("Coudln't create configuration file!");

    if (!fs::exists(HOME/master_src) && core::new_file(HOME/master_src)) {
        core::debug("Created unexistent master config file!");
    }
    core::debug("Created new config file in: ", (config_d/name/"config").string());

    // constants
    const fs::path repo_d = core::parsePathTilde(data_d/name);
    const fs::path config = core::parsePathTilde(config_d/name);

    // create data(also repository) directory and a config-reference
    core::ensure_directories(repo_d/data_cfgref);
    // create and push github repo
    core::CmdStream {}
        .add("cd {}", repo_d.string())
        .add("git init")
        .add("touch .gitkeep")
        .add("git add .gitkeep")
        .add("git commit -m {}", initial_commit_message)
        .add("gh repo create {} --{} --source={} --remote=origin --push",
            repo_name, is_public?"public":"private", repo_d.string())
    .run(true, false);

    core::debug("Writing new profile configurations to master config");
    MasterConfigParser master_cfman;
    if (auto rep_parse = master_cfman.rParse(HOME/master_src)) {
        rep_parse.printOnBad().terminateOnBad();
    } else {
        if (auto rep_eval = master_cfman.rEval()) {
            rep_eval.printComplains();
        }
    }

    // add profile to config
    master_cfman.wAddProfile(Profile{
        name, core::make_repo_url(github_name, repo_name), is_public, is_external
    }).printOnBad();

    // activate new profile and save configuration
    master_cfman.wActivateProfile(name).printOnBad();
    master_cfman.wSaveConfig(HOME/master_src).printOnBad();

    reloadConfig().printComplains();
    return Report::Good();
}



Report CM::deleteProfile(const strview profile_name) {
    if (!profileExists(profile_name)) {
        return Report::Bad("Can't delete '{}', it doesn't exist!", profile_name);
    }
    auto rep = validateProfileName(profile_name.data());
    if (rep.error()) {
        rep.printComplains();
        return Report::Bad("Couldn't delete profile!");
    }

    bool is_active = activeProf() == getProfileByName(profile_name)->name;

    MasterConfigParser master_cfman;
    master_cfman.rParse(HOME/master_src).printOnBad();
    master_cfman.rEval().printComplains();

    if (is_active) {
        master_cfman.wActivateProfile(Profile::NOT)
            .printOnBad().terminateOnBad();
    }
    auto report = master_cfman.wRemoveProfile(profile_name);

    if (report.success()) {
        master_cfman
            .wSaveConfig(HOME/master_src)
            .printOnBad()
            .terminateOnBad();
    } else {  // failed wRemoveProfile
        report.printOnBad();
        return report.Bad("Couldn't delete profile!");
    }

    return Report::Good();
}



// Set current dotty profile
Report CM::setActiveProfile(const strview name) {
    Report report;
    MasterConfigParser master_cfman;

    if (noProfilesExist()) {
        return report.Bad("Can't set active profile: No profiles exist yet!");
    }
    else if (name!=Profile::NOT && !profileExists(name)) {
        return report.Bad("Can't switch to '{}': Profile doesn't exist!", name);
    }
    else if (m_current_profile.name == name.data()) {
        report.addComplain("profile '{}' is already active", name);
        return report.Good();
    }

    master_cfman.rParse(HOME/master_src).printComplains();
    master_cfman.rEval().printComplains();
    if (auto report = master_cfman.wActivateProfile(name)) {
        report.printOnBad();
    };
    master_cfman.wSaveConfig(HOME/master_src).printOnBad();
    // will terminate on problems such as same named profiles
    master_cfman.rValidateConfig().printOnBad().terminateOnBad();
    reloadConfig().mute();

    if (auto* found_prof = getProfileByName(name)) {
        m_current_profile = *found_prof;
    } else {  // p_prof = nullptr
        return report.Bad("Couldn't find profile '{}'", name);
    }
    return report.Good();
}


Report CM::listProfiles(bool name, bool repo, bool url, bool gh) {
    Report rep;

    for (uint32 i=0;  i < m_profiles.size();  ++i) {
        auto prof = m_profiles[i];
        // if the current iterated profile is active one
        bool active = activeProf() == getProfileByName(prof.name)->name;

        std::string msg;
        // TODO: make them switch-case
        if (active) {
            if(name) msg += " | \033[32m" + prof.name + "\033[0m";
            if(repo) msg += " | \033[34m" + core::repo_from_url(prof.repo_url) + "\033[0m";
            if(url)  msg += " | \033[4;36m" + prof.repo_url + "\033[0m";
            if(gh)   msg += " | \033[38m" + core::gh_host_from_url(prof.repo_url) + "\033[0m";
        } else {
            if(name) msg += " | " + prof.name + "";
            if(repo) msg += " | " + core::repo_from_url(prof.repo_url) + "";
            if(url)  msg += " | \033[4m" + prof.repo_url + "\033[0m";
            if(gh)   msg += " | " + core::gh_host_from_url(prof.repo_url) + "";
        }

        core::print(i+1, ": ", msg ," |\n");
    }

    return rep;
}


// 1. deletes config storage
// 2. removes master config
// 3. removes active profile's config
// Report Cfman::cleanConfigs(bool config, bool storage) {
//     Report report;
//     fs::path target_path;

//     if (master) {
//         target_path = HOME/master_src;
//         if (fs::exists(target_path)) {
//             core::print(":: Resetting master config\n");
//             std::ofstream master_cfg(HOME/master_src, std::ios::out);  // clears file
//         } else {
//             report.addComplain("Path does not exist: {}", target_path.string());
//         }
//     }


//     if (noProfilesExist()) {
//         return Report::Bad("Can't clean profile configs: No profiles exist");
//     }
//     else if (activeProf() == NO_PROFILE) {
//         return Report::Bad("Can't clean profile configs: No profiles are active");
//     }

//     if (config) {
//         target_path = config_d/activeProf();
//         if (fs::exists(target_path)) {
//             core::print(":: Cleaning profile configs: ", target_path.string());
//             core::remove_dir_contents_recursive(target_path, {config_src});
//             // clear config file
//             std::ofstream master_cfg(config_d/activeProf()/config_src, std::ios::out);
//         } else {
//             report.addComplain("Path does not exist: {}", target_path.string());
//         }
//     }

//     if (storage) {
//         target_path = data_d/activeProf();
//         if (fs::exists(target_path)) {
//             core::print(":: Removing config storage contents: ", target_path.string());
//             std::pair ratio = core::remove_dir_contents_recursive(target_path);
//             if (!(ratio.first == ratio.second)) {  // not all content is removed
//                 report.addComplain("Removed ", ratio.first, "items out of ", ratio.second, "\n");
//             }
//             else core::debug("All ", ratio.first, " items removed");
//         } else {
//             core::debug("Path does not exist: ", target_path.string());
//         }
//     }

//     return report;
// }


bool CM::detectPreinitConfig() {
    std::ifstream master(HOME/master_src, std::ios::in);
    if (!master) return false;  // doesn't even exist
    else if (fs::is_empty(HOME/master_src)) return false;
    return true;
}


Report CM::reloadConfig() {
    core::debug("", __FUNCTION__, "()...");

    core::debug("Loading master config..\n");
    std::ifstream master(HOME/master_src);
    MasterConfigParser master_cfman;
    master_cfman.rParse(HOME/master_src).printOnBad();
    master_cfman.rEval().printComplains();
    master_cfman.rValidateConfig().printOnBad();

    // register loaded profiles
    m_profiles = master_cfman.profiles;

    // set active profile based on the config
    auto it = master_cfman.vars.find(MasterConfigParser::P_ACTIVE_PROF);
    if (it != master_cfman.vars.end() && (it->second != Profile::NOT)) {
        if (Profile* found_prof = getProfileByName(strview(it->second))) {
            m_current_profile = *found_prof;
        }
    } else {
        return Report::Bad("Couldn't find active profile");
    }

    return Report::Good();
}


// Load dotty configuration and debug
void CM::load(bool reg) {
    core::debug("", __FUNCTION__, "()...");

    std::string active_prof = activeProf();
    fs::path master_path = HOME/master_src;

    // Create needed directories&&files if not exist
    if(reg) if (!fs::exists(master_path)) core::new_file(master_path);
    core::ensure_directories(config_d);
    core::ensure_directories(data_d);
    for (auto& prof : m_profiles) {
        core::ensure_directories(config_d/prof.name);
        if (!fs::exists(config_d/prof.name/config_src)) core::new_file(config_d/prof.name/config_src);
        core::ensure_directories(data_d/prof.name/data_cfgref);
    }

    core::debug("Loading master config..\n");
    std::ifstream master(master_path);
    MasterConfigParser mcparser;
    mcparser.rParse(master_path).printOnBad();
    mcparser.rEval().printComplains();
    mcparser.rValidateConfig().printOnBad();

    // load profiles
    m_profiles = mcparser.profiles;

    // set active profile based on the config
    auto it = mcparser.vars.find(MasterConfigParser::P_ACTIVE_PROF);
    if (it != mcparser.vars.end()) {
        setActiveProfile(it->second.data()).mute();
    } else if (!reg) core::terminate("dotty.load: setProfile(it->second): Error!");
}



// Copy all source files to destination files, pairs defined by a member
std::array<std::vector<SrcDest>, 8>
CM::systemToRepo()
{
    static std::vector<SrcDest> succeed_cp_f; succeed_cp_f.clear();
    static std::vector<SrcDest> succeed_cp_d; succeed_cp_d.clear();
    static std::vector<SrcDest> succeed_ln_f; succeed_ln_f.clear();
    static std::vector<SrcDest> succeed_ln_d; succeed_ln_d.clear();
    static std::vector<SrcDest> succeed_su_cp_f; succeed_su_cp_f.clear();
    static std::vector<SrcDest> succeed_su_cp_d; succeed_su_cp_d.clear();
    static std::vector<SrcDest> succeed_su_ln_f; succeed_su_ln_f.clear();
    static std::vector<SrcDest> succeed_su_ln_d; succeed_su_ln_d.clear();

    COMPTIME_STR ERR = "Skipping target: ";
    auto should_skip = [ERR](const fs::path& src, const fs::path& dest, bool accept_dirs) ->bool {
        bool signal_skip = false;
        const std::string& dest_str = dest.string();
        if (dest.is_absolute()) {
            core::print(ERR, "destination should be relative path!\n"); signal_skip = true;
        }
        else if (!accept_dirs && (
            fs::directory_entry(src).is_directory() || fs::directory_entry(dest).is_directory()
        )){
            core::print(ERR, "neither source nor destination can be written as a directory!\n"); signal_skip = true;
        }
        else if (dest_str.ends_with("/")) {
            core::print(ERR, "path has trailing '/'\n"); signal_skip = true;
        }
        else if (dest_str.starts_with("./") || dest_str.starts_with("../")) {
            core::print(ERR, "path starts with illegal character set\n"); signal_skip = true;
        }
        return signal_skip;
    };

    // prime the sudo credential cache ONCE up front instead of prompting per-file
    bool have_sudo_targets = !sudo_files_to_copy.empty() || !sudo_files_to_link.empty()
                           || !sudo_dirs_to_copy.empty()  || !sudo_dirs_to_link.empty();
    if (have_sudo_targets) {
        core::print("This profile has @allow-sudo entries — you may be asked for your password.\n");
        core::CmdStream{}.add("sudo -v").run(false, false, true);
    }

    fs::path repo_d = data_d/activeProf();

    // COPY-FILES
    for (auto [src, dest] : files_to_copy) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        try {
            fs::copy_file(src, dest, fs::copy_options::update_existing);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n"); continue;
        }
        succeed_cp_f.emplace_back(src, dest);
    }
    // LINK-FILES
    for (auto [src, dest] : files_to_link) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        try {
            fs::create_symlink(src, dest);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n"); continue;
        }
        succeed_ln_f.emplace_back(src, dest);
    }
    // COPY-DIRECTORIES
    for (auto [src, dest] : dirs_to_copy) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        try {
            core::copy_directory(src, dest, true);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n"); continue;
        }
        succeed_cp_d.emplace_back(src, dest);
    }
    // LINK-DIRECTORIES
    for (auto [src, dest] : dirs_to_link) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        try {
            fs::create_directory_symlink(src, dest);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n"); continue;
        }
        succeed_ln_d.emplace_back(src, dest);
    }

    // SUDO-COPY-FILES — our own fs:: calls can't escalate privilege, so shell out
    for (auto [src, dest] : sudo_files_to_copy) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo cp \"{}\" \"{}\"", src.string(), dest.string())
        .run(false, false, true);
        if (rc != 0) { core::print(ERR, "sudo cp failed for '", src.string(), "'\n"); continue; }
        succeed_su_cp_f.emplace_back(src, dest);
    }
    // SUDO-LINK-FILES
    for (auto [src, dest] : sudo_files_to_link) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo ln -sf \"{}\" \"{}\"", src.string(), dest.string())
        .run(false, false, true);
        if (rc != 0) { core::print(ERR, "sudo ln failed for '", src.string(), "'\n"); continue; }
        succeed_su_ln_f.emplace_back(src, dest);
    }
    // SUDO-COPY-DIRECTORIES
    for (auto [src, dest] : sudo_dirs_to_copy) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo cp -r \"{}\" \"{}\"", src.string(), dest.string())
        .run(false, false, true);
        if (rc != 0) { core::print(ERR, "sudo cp -r failed for '", src.string(), "'\n"); continue; }
        succeed_su_cp_d.emplace_back(src, dest);
    }
    // SUDO-LINK-DIRECTORIES
    for (auto [src, dest] : sudo_dirs_to_link) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo ln -sf \"{}\" \"{}\"", src.string(), dest.string())
        .run(false, false, true);
        if (rc != 0) { core::print(ERR, "sudo ln failed for '", src.string(), "'\n"); continue; }
        succeed_su_ln_d.emplace_back(src, dest);
    }

    return std::array<std::vector<SrcDest>, 8>{
        succeed_cp_f, succeed_ln_f, succeed_cp_d, succeed_ln_d,
        succeed_su_cp_f, succeed_su_ln_f, succeed_su_cp_d, succeed_su_ln_d
    };
}



// Copy/link files/directories from repo(config storage) to their system targets
void CM::repoToSystem()
{
    COMPTIME_STR ERR = "Skipping target: ";

    bool have_sudo_targets = (
        !sudo_files_to_copy.empty() || !sudo_files_to_link.empty() ||
        !sudo_dirs_to_copy.empty()  || !sudo_dirs_to_link.empty()
    );

    if (have_sudo_targets) {
        core::print("This profile has @allow-sudo entries — you may be asked for your password.\n");
        core::CmdStream{}.add("sudo -v").run(false, false, true);
    }

    // COPY-FILES
    for (auto [src, dest] : files_to_copy) {
        core::ensure_directories(dest.parent_path());
        try {
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n");
        }
    }
    // LINK-FILES
    for (auto [src, dest] : files_to_link) {
        core::ensure_directories(dest.parent_path());
        try {
            if (fs::exists(dest) || fs::is_symlink(dest)) fs::remove(dest);
            fs::create_symlink(src, dest);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n");
        }
    }
    // COPY-DIRECTORIES
    for (auto [src, dest] : dirs_to_copy) {
        core::ensure_directories(dest.parent_path());
        try {
            core::copy_directory(src, dest, true);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n");
        }
    }
    // LINK-DIRECTORIES
    for (auto [src, dest] : dirs_to_link) {
        core::ensure_directories(dest.parent_path());
        try {
            if (fs::exists(dest) || fs::is_symlink(dest)) fs::remove(dest);
            fs::create_directory_symlink(src, dest);
        } catch (const std::exception& e) {
            core::print(ERR, e.what(), "\n");
        }
    }

    // SUDO-COPY-FILES
    for (auto [src, dest] : sudo_files_to_copy) {
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo cp \"{}\" \"{}\"", src.string(), dest.string())
        .run(false, false, true);
        if (rc != 0) core::print(ERR, "sudo cp failed for '", src.string(), "'\n");
    }
    // SUDO-LINK-FILES
    for (auto [src, dest] : sudo_files_to_link) {
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo rm -f \"{}\"", dest.string())
            .add("sudo ln -s \"{}\" \"{}\"", src.string(), dest.string())
        .run(true, false, true);
        if (rc != 0) core::print(ERR, "sudo ln failed for '", src.string(), "'\n");
    }
    // SUDO-COPY-DIRECTORIES
    for (auto [src, dest] : sudo_dirs_to_copy) {
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo cp -r \"{}\" \"{}\"", src.string(), dest.string())
        .run(false, false, true);
        if (rc != 0) core::print(ERR, "sudo cp -r failed for '", src.string(), "'\n");
    }
    // SUDO-LINK-DIRECTORIES
    for (auto [src, dest] : sudo_dirs_to_link) {
        core::ensure_directories(dest.parent_path());
        int32 rc = core::CmdStream{}
            .add("sudo rm -rf \"{}\"", dest.string())
            .add("sudo ln -s \"{}\" \"{}\"", src.string(), dest.string())
        .run(true, false, true);
        if (rc != 0) core::print(ERR, "sudo ln failed for '", src.string(), "'\n");
    }
}



ConfigManager dotty;
