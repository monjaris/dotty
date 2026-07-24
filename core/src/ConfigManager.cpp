#include "ConfigManager.hpp"


Report ConfigManager::validateProfileName(const std::string& name) {
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


Report ConfigManager::validateRepoName(const std::string& repo) {
    if (repo.empty()) {
        return Report::Bad("Repo name should not be empty");
    }

    return Report::Good();
}


bool ConfigManager::noProfilesExist() {
    return m_profiles.size() == 0;
}


bool ConfigManager::profileExists(const strview profile_name) {
    for (const Profile& prof : m_profiles) {
        if (prof.name == profile_name) {
            return true;
        }
    }
    return false;
}


Profile* ConfigManager::getProfileByName(const strview prof_name) {
    for (uint32 i=0;  i < m_profiles.size();  ++i) {
        if (m_profiles[i].name == prof_name) {
            return &m_profiles[i];
        }
    }
    // not found
    return nullptr;
}


// Get current profile as string
std::string ConfigManager::activeProf() {
    std::string profile_name = m_current_profile.name;
    return profile_name;
}



// Create a folder and register a new profile
Report ConfigManager::newProfile(
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
    if (!cm::new_file(config_d/name/config_src)) return Report::Bad("Coudln't create configuration file!");

    if (!fs::exists(HOME/master_src) && cm::new_file(HOME/master_src)) {
        cm::debug("Created unexistent master config file!");
    }
    cm::debug("Created new config file in: ", (config_d/name/"config").string());

    // constants
    const fs::path repo_d = cm::parsePathTilde(data_d/name);
    const fs::path config = cm::parsePathTilde(config_d/name);

    // create data(also repository) directory and a config-reference
    cm::ensure_directories(repo_d/data_cfgref);
    // create and push github repo
    cm::CmdStream {}
        .add("cd {}", repo_d.string())
        .add("git init")
        .add("touch .gitkeep")
        .add("git add .gitkeep")
        .add("git commit -m {}", initial_commit_message)
        .add("gh repo create {} --{} --source={} --remote=origin --push",
            repo_name, is_public?"public":"private", repo_d.string())
    .run(" && ", false);

    cm::debug("Writing new profile configurations to master config");
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
        name, cm::make_repo_url(github_name, repo_name), is_public, is_external
    }).printOnBad();

    // activate new profile and save configuration
    master_cfman.wActivateProfile(name).printOnBad();
    master_cfman.wSaveConfig(HOME/master_src).printOnBad();

    reloadConfig().printComplains();
    return Report::Good();
}



Report ConfigManager::deleteProfile(const strview profile_name) {
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
Report ConfigManager::setActiveProfile(const strview name) {
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


Report ConfigManager::listProfiles(bool name, bool repo, bool url, bool gh) {
    Report rep;

    for (uint32 i=0;  i < m_profiles.size();  ++i) {
        auto prof = m_profiles[i];
        // if the current iterated profile is active one
        bool active = activeProf() == getProfileByName(prof.name)->name;

        std::string msg;
        // TODO: make them switch-case
        if (active) {
            if(name) msg += " | \033[32m" + prof.name + "\033[0m";
            if(repo) msg += " | \033[34m" + cm::repo_from_url(prof.repo_url) + "\033[0m";
            if(url)  msg += " | \033[4;36m" + prof.repo_url + "\033[0m";
            if(gh)   msg += " | \033[38m" + cm::gh_host_from_url(prof.repo_url) + "\033[0m";
        } else {
            if(name) msg += " | " + prof.name + "";
            if(repo) msg += " | " + cm::repo_from_url(prof.repo_url) + "";
            if(url)  msg += " | \033[4m" + prof.repo_url + "\033[0m";
            if(gh)   msg += " | " + cm::gh_host_from_url(prof.repo_url) + "";
        }

        cm::print(i+1, ": ", msg ," |\n");
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
//             cm::print(":: Resetting master config\n");
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
//             cm::print(":: Cleaning profile configs: ", target_path.string());
//             cm::remove_dir_contents_recursive(target_path, {config_src});
//             // clear config file
//             std::ofstream master_cfg(config_d/activeProf()/config_src, std::ios::out);
//         } else {
//             report.addComplain("Path does not exist: {}", target_path.string());
//         }
//     }

//     if (storage) {
//         target_path = data_d/activeProf();
//         if (fs::exists(target_path)) {
//             cm::print(":: Removing config storage contents: ", target_path.string());
//             std::pair ratio = cm::remove_dir_contents_recursive(target_path);
//             if (!(ratio.first == ratio.second)) {  // not all content is removed
//                 report.addComplain("Removed ", ratio.first, "items out of ", ratio.second, "\n");
//             }
//             else cm::debug("All ", ratio.first, " items removed");
//         } else {
//             cm::debug("Path does not exist: ", target_path.string());
//         }
//     }

//     return report;
// }


bool ConfigManager::detectPreinitConfig() {
    std::ifstream master(HOME/master_src, std::ios::in);
    if (!master) return false;  // doesn't even exist
    else if (fs::is_empty(HOME/master_src)) return false;
    return true;
}


Report ConfigManager::reloadConfig() {
    cm::debug("", __FUNCTION__, "()...");

    cm::debug("Loading master config..\n");
    std::ifstream master(HOME/master_src);
    MasterConfigParser master_cfman;
    master_cfman.rParse(HOME/master_src).printOnBad();
    master_cfman.rEval().printComplains();
    master_cfman.rValidateConfig().printOnBad();

    // register loaded profiles
    m_profiles = master_cfman.profiles;

    // set active profile based on the config
    auto it = master_cfman.vars.find(MasterConfigParser::P_ACTIVE_PROF);
    if (it != master_cfman.vars.end() && (it->second == Profile::NOT)) {
        if (Profile* found_prof = getProfileByName(strview(it->second))) {
            m_current_profile = *found_prof;
        }
    } else {
        return Report::Bad("Couldn't find active profile");
    }

    return Report::Good();
}


// Load dotty configuration and debug
void ConfigManager::load(bool reg) {
    cm::debug("", __FUNCTION__, "()...");

    std::string prof = activeProf();
    fs::path master_path = HOME/master_src;

    // Create needed directories&&files if not exist
    if(reg) if (!fs::exists(master_path)) cm::new_file(master_path);
    cm::ensure_directories(config_d);
    cm::ensure_directories(data_d);
    for (auto& profile : m_profiles) {
        cm::ensure_directories(config_d/profile.name);
        if (!fs::exists(config_d/prof/config_src)) cm::new_file(config_d/prof/config_src);
        cm::ensure_directories(data_d/profile.name/data_cfgref);
    }

    cm::debug("Loading master config..\n");
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
    } else if (!reg) cm::terminate("dotty.load: setProfile(it->second): Error!");
}


// Copy all source files to destination files, pairs defined by a member
std::array<std::vector<SrcDest>, 4>
ConfigManager::systemToRepo() {

    static std::vector<SrcDest> succeed_cp_f;
    static std::vector<SrcDest> succeed_cp_d;
    static std::vector<SrcDest> succeed_ln_f;
    static std::vector<SrcDest> succeed_ln_d;

    COMPTIME_STR ERR = "Skipping target: ";
    auto should_skip = [ERR](const fs::path& src, const fs::path& dest, bool accept_dirs) ->bool {
        bool signal_skip = false;
        const std::string& dest_str = dest.string();
        // destination should be relative to the repo
        if (dest.is_absolute()) {
            cm::print(
                ERR, "destination should be relative path!\n"
            ); signal_skip = true;
        }
        // Neither source nor destination can be written as a directory
        else if (!accept_dirs && (
            fs::directory_entry(src).is_directory() || fs::directory_entry(dest).is_directory()
        )){
            cm::print(
                ERR, "neither source nor destination can be written as a directory!\n"
            ); signal_skip = true;
        }
        // Dont allow trailing '/'
        else if (dest_str.ends_with("/")) {
            cm::print(
                ERR, "path has trailing '/'\n"
            ); signal_skip = true;
        }
        else if (dest_str.starts_with("./") || dest_str.starts_with("../")) {
            cm::print(
                ERR, "path starts with illegal character set\n"
            ); signal_skip = true;
        }
        // return value will signal if called should 'continue' loop
        return signal_skip;
    }; // lambda should_skip;


    fs::path repo_d = data_d/activeProf();

    // COPY-FILES
    for (auto [src, dest] : files_to_copy) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            fs::copy_file(src, dest, fs::copy_options::update_existing);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_cp_f.emplace_back(src, dest);
    }
    // LINK-FILES
    for (auto [src, dest] : files_to_link) {
        if (should_skip(src, dest, false)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            fs::create_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_ln_f.emplace_back(src, dest);
    }
    // COPY-DIRECTORIES
    for (auto [src, dest] : dirs_to_copy) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            cm::copy_directory(src, dest, true);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_cp_d.emplace_back(src, dest);
    }
    // LINK-DIRECTORIES
    for (auto [src, dest] : dirs_to_link) {
        if (should_skip(src, dest, true)) continue;
        dest = repo_d / dest;
        cm::ensure_directories(dest.parent_path());
        try {
            fs::create_directory_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what());
        }
        succeed_ln_d.emplace_back(src, dest);
    }

    // return paths which succeed to be copied/linked
    return std::array<std::vector<SrcDest>, 4>{
        succeed_cp_f, succeed_ln_f, succeed_cp_d, succeed_ln_d
    };
}


// Copy/link files/directories from repo(config storage) to their system targets
void ConfigManager::repoToSystem() {
    COMPTIME_STR ERR = "Skipping target: ";
    // COPY-FILES
    for (auto [src, dest] : files_to_copy) {
        cm::ensure_directories(dest.parent_path());
        try {
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
    // LINK-FILES
    for (auto [src, dest] : files_to_link) {
        cm::ensure_directories(dest.parent_path());
        try {
            // check if dest's dereference exists or if dest is symlink
            if (fs::exists(dest) || fs::is_symlink(dest)) fs::remove(dest);
            fs::create_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
    // COPY-DIRECTORIES
    for (auto [src, dest] : dirs_to_copy) {
        cm::ensure_directories(dest.parent_path());
        try {
            cm::copy_directory(src, dest, true);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
    // LINK-DIRECTORIES
    for (auto [src, dest] : dirs_to_link) {
        cm::ensure_directories(dest.parent_path());
        try {
            if (fs::exists(dest) || fs::is_symlink(dest)) fs::remove(dest);
            fs::create_directory_symlink(src, dest);
        } catch (const std::exception& e) {
            cm::print(ERR, e.what(), "\n");
        }
    }
}

ConfigManager dotty;
