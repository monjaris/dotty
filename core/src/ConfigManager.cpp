#include "ConfigManager.hpp"
#include "CmdStream.hpp"
#include <algorithm>

using CM = ConfigManager;

CM::ConfigManager()
    : HOME([&]() -> fs::path {
          const char* h = core::os::userHomePath();
          if (h == nullptr || h[0] == '\0') return fs::path{};
          return fs::path(h);
      }())
    , config_d()
    , data_d()
{
    if (HOME.empty()) {
        // Leave paths empty; load()/init will report a clear error.
        return;
    }
    // Prefer XDG_CONFIG_HOME when set; otherwise ~/.config/dotty
    const char* xdg = ::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        config_d = fs::path(xdg) / "dotty";
    } else {
        config_d = HOME / ".config" / "dotty";
    }
    data_d = HOME / ".local" / "share" / "dotty";
}

Report CM::validateProfileName(const std::string& name) {
    if (name == Profile::NOT) {
        return Report::Bad("Profile can't be assigned to profile sentinel('{}')", Profile::NOT);
    }
    else if (name.empty()) {
        return Report::Bad("Profile name can't be empty");
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
    if (repo.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._")
        != std::string::npos) {
        return Report::Bad("Repo name contains illegal character");
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

    if (auto v = validateProfileName(name); v.error()) return v;
    if (auto v = validateRepoName(repo_name); v.error()) return v;
    if (profileExists(name)) return Report::Bad("{} '{}': Profile already exists.", err, name);

    if (HOME.empty()) {
        return Report::Bad("{}: HOME is not set", err);
    }

    std::error_code ec;
    fs::create_directories(config_d / name, ec);
    if (ec) {
        return Report::Bad(
            "Couldn't create config directory '{}': {}",
            (config_d / name).string(), ec.message()
        );
    }

    const fs::path cfg_file = config_d / name / config_src;
    if (!fs::exists(cfg_file, ec)) {
        if (!core::new_file(cfg_file)) {
            return Report::Bad("Couldn't create configuration file '{}'", cfg_file.string());
        }
    }

    const fs::path master_path = HOME / master_src;
    if (!fs::exists(master_path, ec) || core::is_file_empty(master_path)) {
        std::ofstream fo(master_path, std::ios::out | std::ios::trunc);
        if (!fo) return Report::Bad("Couldn't create master config '{}'", master_path.string());
        fo << "active-profile = \"[NIL-PROFILE]\"\n"
           << "config-editor = \"\"\n"
           << "profile = []\n";
        if (!fo) return Report::Bad("Couldn't write master config '{}'", master_path.string());
    }

    const fs::path repo_d = data_d / name;
    if (!core::ensure_directories(repo_d / data_cfgref)) {
        return Report::Bad("Couldn't create data directory '{}'", (repo_d / data_cfgref).string());
    }

    if (!core::os::in_path("git")) {
        return Report::Bad("{}: 'git' is not installed", err);
    }
    if (!core::os::in_path("gh")) {
        return Report::Bad("{}: 'gh' (GitHub CLI) is not installed", err);
    }

    const char* commit = (initial_commit_message && initial_commit_message[0])
        ? initial_commit_message
        : "Initial commit of this configuration profile";

    int32 git_rc = core::CmdStream {}
        .add("cd {}", core::shell_quote(repo_d.string()))
        .add("git init")
        .add("touch .gitkeep")
        .add("git add .gitkeep")
        .add("git commit -m {}", core::shell_quote(commit))
        .add("gh repo create {} --{} --source={} --remote=origin --push",
            core::shell_quote(repo_name), is_public ? "public" : "private",
            core::shell_quote(repo_d.string()))
    .run(true, false);

    if (git_rc != 0) {
        return Report::Bad(
            "{} '{}': git/gh pipeline failed (exit {}). "
            "Local directories were created; fix the remote and retry, or delete the profile.",
            err, name, git_rc
        );
    }

    MasterConfigParser master_cfman;
    if (auto rep_parse = master_cfman.rParse(master_path); rep_parse.error()) {
        return Report::Bad("{}: failed to parse master config: {}", err, rep_parse.m_msg);
    }
    master_cfman.rEval().printComplains();

    if (auto add = master_cfman.wAddProfile(Profile{
        name, core::make_repo_url(github_name, repo_name), is_public, is_external
    }); add.error()) {
        return add;
    }

    if (auto act = master_cfman.wActivateProfile(name); act.error()) {
        return act;
    }
    if (auto save = master_cfman.wSaveConfig(master_path); save.error()) {
        return save;
    }

    if (auto rel = reloadConfig(); rel.error()) {
        rel.printComplains();
    }
    return Report::Good();
}



Report CM::deleteProfile(const strview profile_name) {
    const std::string name{profile_name};

    if (auto v = validateProfileName(name); v.error()) {
        return v;
    }
    if (!profileExists(name)) {
        return Report::Bad("Can't delete '{}', it doesn't exist!", name);
    }

    const bool was_active = (activeProf() == name);

    MasterConfigParser master_cfman;
    const fs::path master_path = HOME / master_src;
    if (auto rp = master_cfman.rParse(master_path); rp.error()) {
        return Report::Bad("Can't delete '{}': failed to parse master config", name);
    }

    if (was_active) {
        if (auto act = master_cfman.wActivateProfile(Profile::NOT); act.error()) {
            return act;
        }
    }

    if (auto rem = master_cfman.wRemoveProfile(name); rem.error()) {
        return rem;
    }

    if (auto save = master_cfman.wSaveConfig(master_path); save.error()) {
        return save;
    }

    // Keep in-memory state in sync so later commands in this process see the deletion.
    m_profiles.erase(
        std::remove_if(m_profiles.begin(), m_profiles.end(),
            [&](const Profile& p) { return p.name == name; }),
        m_profiles.end()
    );
    if (was_active) {
        m_current_profile = Profile{};
    }

    return Report::Good();
}



// Set current dotty profile
Report CM::setActiveProfile(const strview name) {
    Report report;

    if (noProfilesExist() && name != Profile::NOT) {
        return Report::Bad("Can't set active profile: No profiles exist yet!");
    }
    else if (name != Profile::NOT && !profileExists(name)) {
        return Report::Bad("Can't switch to '{}': Profile doesn't exist!", name);
    }
    else if (m_current_profile.name == name) {
        return Report::Good();
    }

    MasterConfigParser master_cfman;
    const fs::path master_path = HOME / master_src;
    if (auto rp = master_cfman.rParse(master_path); rp.error()) {
        return Report::Bad("Can't switch profile: failed to parse master config");
    }
    master_cfman.rEval().printComplains();

    if (auto act = master_cfman.wActivateProfile(name); act.error()) {
        return act;
    }
    if (auto save = master_cfman.wSaveConfig(master_path); save.error()) {
        return save;
    }

    if (name == Profile::NOT) {
        m_current_profile = Profile{};
        return Report::Good();
    }

    if (auto* found_prof = getProfileByName(name)) {
        m_current_profile = *found_prof;
        return Report::Good();
    }
    return Report::Bad("Couldn't find profile '{}'", name);
}


Report CM::listProfiles(bool name, bool repo, bool url, bool gh) {
    Report rep;

    for (uint32 i=0;  i < m_profiles.size();  ++i) {
        auto prof = m_profiles[i];
        // if the current iterated profile is active one
        bool active = (activeProf() == prof.name);

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
    std::error_code ec;
    const fs::path master_path = HOME / master_src;
    if (!fs::exists(master_path, ec) || ec) return false;
    return !core::is_file_empty(master_path);
}


Report CM::reloadConfig() {
    core::debug("", __FUNCTION__, "()...");

    if (HOME.empty()) {
        return Report::Bad("HOME is not set");
    }

    const fs::path master_path = HOME / master_src;
    std::error_code ec;
    if (!fs::exists(master_path, ec) || core::is_file_empty(master_path)) {
        m_profiles.clear();
        m_current_profile = Profile{Profile::NOT, "", false, false};
        return Report::Good();
    }

    core::debug("Loading master config..\n");
    MasterConfigParser master_cfman;
    if (auto r = master_cfman.rParse(master_path); r.error()) {
        r.printComplains();
        return Report::Bad("Failed to parse master config");
    }
    master_cfman.rEval().printComplains();
    master_cfman.rValidateConfig().printComplains();

    m_profiles = master_cfman.profiles;

    auto it = master_cfman.vars.find(MasterConfigParser::P_ACTIVE_PROF);
    if (it != master_cfman.vars.end() && it->second != Profile::NOT) {
        if (Profile* found_prof = getProfileByName(strview(it->second))) {
            m_current_profile = *found_prof;
        } else {
            m_current_profile = Profile{Profile::NOT, "", false, false};
        }
    } else {
        m_current_profile = Profile{Profile::NOT, "", false, false};
    }

    return Report::Good();
}



// Load dotty configuration. Never throws; returns Report on failure.
Report CM::load(bool reg) {
    core::debug("", __FUNCTION__, "()...");

    if (HOME.empty()) {
        return Report::Bad(
            "HOME environment variable is not set. "
            "Cannot locate user configuration directories."
        );
    }

    fs::path master_path = HOME / master_src;

    // Create needed directories && files if requested
    if (reg) {
        std::error_code ec;
        if (!fs::exists(master_path, ec)) {
            core::new_file(master_path);
        }
        if (!core::ensure_directories(config_d)) {
            return Report::Bad("Could not create config directory: '{}'", config_d.string());
        }
        if (!core::ensure_directories(data_d)) {
            return Report::Bad("Could not create data directory: '{}'", data_d.string());
        }
    }

    for (auto& prof : m_profiles) {
        core::ensure_directories(config_d / prof.name);
        std::error_code ec;
        if (!fs::exists(config_d / prof.name / config_src, ec)) {
            core::new_file(config_d / prof.name / config_src);
        }
        core::ensure_directories(data_d / prof.name / data_cfgref);
    }

    // Master config may not exist yet (fresh init) - that is fine.
    std::error_code ec;
    if (!fs::exists(master_path, ec) || core::is_file_empty(master_path)) {
        core::debug("Master config missing or empty - no profiles loaded yet.\n");
        m_profiles.clear();
        m_current_profile = Profile{Profile::NOT, "", false, false};
        return Report::Good();
    }

    core::debug("Loading master config..\n");
    MasterConfigParser mcparser;
    if (auto r = mcparser.rParse(master_path); r.error()) {
        r.printComplains();
        return Report::Bad("Failed to parse master config '{}'", master_path.string());
    }
    mcparser.rEval().printComplains();
    mcparser.rValidateConfig().printComplains();

    m_profiles = mcparser.profiles;

    // Set active profile directly - avoid re-entrant setActiveProfile() during load.
    auto it = mcparser.vars.find(MasterConfigParser::P_ACTIVE_PROF);
    if (it != mcparser.vars.end() && it->second != Profile::NOT) {
        if (Profile* found = getProfileByName(it->second)) {
            m_current_profile = *found;
        } else {
            m_current_profile = Profile{};
        }
    } else {
        m_current_profile = Profile{};
    }

    return Report::Good();
}


// Run collected @exec commands via posix_spawn (no shell).
Report CM::runExecCommands() {
    Report report;
    if (exec_commands.empty()) return report;

    core::print("Running ", exec_commands.size(), " @exec command(s)...\n");
    for (const auto& cmdline : exec_commands) {
        core::print("  $ ", cmdline, "\n");

        // Tokenize with wordexp (POSIX, no shell meta-execution of pipes/redirs
        // beyond word expansion). Then posix_spawnp the resulting argv.
        core::os::CmdTokens tokens(cmdline.c_str());
        if (!tokens.parse(WRDE_NOCMD)) {
            report.addComplain("@exec: failed to parse command: '{}'", cmdline);
            continue;
        }

        // Build argv for spawn_child
        const char* const* words = tokens.words();
        if (tokens.count() == 0 || words[0] == nullptr) {
            report.addComplain("@exec: empty argv for '{}'", cmdline);
            continue;
        }

        pid_t pid = core::os::spawn_child(words[0], words + 1);
        if (pid == -1) {
            report.addComplain("@exec: posix_spawn failed for '{}'", cmdline);
            continue;
        }
        int32 status = core::os::wait_proc(pid);
        if (status != 0) {
            report.addComplain("@exec: '{}' exited with status {}", cmdline, status);
        }
    }
    return report;
}


// Copy/link files from system targets into the profile repository.
// Never throws; returns Report summarizing failures.
Report CM::systemToRepo()
{
    Report report;
    COMPTIME_STR ERR = "Skipping target: ";

    auto should_skip = [&](const fs::path& src, const fs::path& dest, bool accept_dirs) -> bool {
        std::error_code ec;
        if (!fs::exists(src, ec) || ec) {
            core::print(ERR, "source does not exist: '", src.string(), "'\n");
            return true;
        }
        const fs::path normalized = dest.lexically_normal();
        if (dest.empty() || dest.is_absolute() || normalized.empty()
            || normalized == "." || normalized.begin()->string() == "..") {
            core::print(ERR, "destination must stay inside the profile repository!\n");
            return true;
        }
        if (!accept_dirs && fs::is_directory(src, ec)) {
            core::print(ERR, "a file mapping cannot use a directory source!\n");
            return true;
        }
        return false;
    };

    bool have_sudo_targets = !sudo_files_to_copy.empty() || !sudo_files_to_link.empty()
                           || !sudo_dirs_to_copy.empty()  || !sudo_dirs_to_link.empty();
    if (have_sudo_targets) {
        core::print("This profile has @sudo entries - you may be asked for your password.\n");
        core::CmdStream{}.add("sudo -v").run(false, false, true);
    }

    fs::path repo_d = data_d / activeProf();
    if (!core::ensure_directories(repo_d)) {
        return Report::Bad("Could not create repository directory: '{}'", repo_d.string());
    }

    auto do_copy_file = [&](const fs::path& src, fs::path dest, bool use_sudo) {
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo cp {} {}", core::shell_quote(src.string()), core::shell_quote(dest.string()))
            .run(false, false, true);
            if (rc != 0) {
                report.addComplain("sudo cp failed for '{}'", src.string());
            } else {
                core::print("  [sudo-copy] ", src.string(), " -> ", dest.string(), "\n");
            }
        } else {
            if (!core::copy_file_safe(src, dest, fs::copy_options::update_existing)) {
                report.addComplain("copy failed for '{}'", src.string());
            } else {
                core::print("  [copy] ", src.string(), " -> ", dest.string(), "\n");
            }
        }
    };

    auto do_link_file = [&](const fs::path& src, fs::path dest, bool use_sudo) {
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo cp -f {} {}", core::shell_quote(src.string()), core::shell_quote(dest.string()))
                .add("sudo rm -f {}", core::shell_quote(src.string()))
                .add("sudo ln -s {} {}", core::shell_quote(dest.string()), core::shell_quote(src.string()))
            .run(true, false, true);
            if (rc != 0) {
                report.addComplain("sudo ln failed for '{}'", src.string());
            } else {
                core::print("  [sudo-link] ", src.string(), " -> ", dest.string(), "\n");
            }
        } else {
            std::error_code ec;
            if (!core::copy_file_safe(src, dest, fs::copy_options::overwrite_existing)) {
                report.addComplain("copy-before-link failed for '{}'", src.string());
                return;
            }
            if (fs::exists(src, ec) || fs::is_symlink(src, ec)) {
                fs::remove(src, ec);
            }
            fs::create_symlink(dest, src, ec);
            if (ec) {
                report.addComplain("symlink failed for '{}': {}", src.string(), ec.message());
            } else {
                core::print("  [link] ", src.string(), " -> ", dest.string(), "\n");
            }
        }
    };

    auto do_copy_dir = [&](const fs::path& src, fs::path dest, bool use_sudo) {
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo cp -r {} {}", core::shell_quote(src.string()), core::shell_quote(dest.string()))
            .run(false, false, true);
            if (rc != 0) {
                report.addComplain("sudo cp -r failed for '{}'", src.string());
            } else {
                core::print("  [sudo-copy-dir] ", src.string(), " -> ", dest.string(), "\n");
            }
        } else {
            if (!core::copy_directory(src, dest, true)) {
                report.addComplain("directory copy failed for '{}'", src.string());
            } else {
                core::print("  [copy-dir] ", src.string(), " -> ", dest.string(), "\n");
            }
        }
    };

    auto do_link_dir = [&](const fs::path& src, fs::path dest, bool use_sudo) {
        dest = repo_d / dest;
        core::ensure_directories(dest.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo cp -r {} {}", core::shell_quote(src.string()), core::shell_quote(dest.string()))
                .add("sudo rm -rf {}", core::shell_quote(src.string()))
                .add("sudo ln -s {} {}", core::shell_quote(dest.string()), core::shell_quote(src.string()))
            .run(true, false, true);
            if (rc != 0) {
                report.addComplain("sudo ln -s (dir) failed for '{}'", src.string());
            } else {
                core::print("  [sudo-link-dir] ", src.string(), " -> ", dest.string(), "\n");
            }
        } else {
            std::error_code ec;
            if (!core::copy_directory(src, dest, false)) {
                report.addComplain("directory copy-before-link failed for '{}'", src.string());
                return;
            }
            if (fs::exists(src, ec) || fs::is_symlink(src, ec)) {
                fs::remove_all(src, ec);
            }
            fs::create_directory_symlink(dest, src, ec);
            if (ec) {
                report.addComplain("directory symlink failed for '{}': {}", src.string(), ec.message());
            } else {
                core::print("  [link-dir] ", src.string(), " -> ", dest.string(), "\n");
            }
        }
    };

    for (auto& [src, dest] : files_to_copy) {
        if (should_skip(src, dest, false)) continue;
        do_copy_file(src, dest, false);
    }
    for (auto& [src, dest] : files_to_link) {
        if (should_skip(src, dest, false)) continue;
        do_link_file(src, dest, false);
    }
    for (auto& [src, dest] : dirs_to_copy) {
        if (should_skip(src, dest, true)) continue;
        do_copy_dir(src, dest, false);
    }
    for (auto& [src, dest] : dirs_to_link) {
        if (should_skip(src, dest, true)) continue;
        do_link_dir(src, dest, false);
    }
    for (auto& [src, dest] : sudo_files_to_copy) {
        if (should_skip(src, dest, false)) continue;
        do_copy_file(src, dest, true);
    }
    for (auto& [src, dest] : sudo_files_to_link) {
        if (should_skip(src, dest, false)) continue;
        do_link_file(src, dest, true);
    }
    for (auto& [src, dest] : sudo_dirs_to_copy) {
        if (should_skip(src, dest, true)) continue;
        do_copy_dir(src, dest, true);
    }
    for (auto& [src, dest] : sudo_dirs_to_link) {
        if (should_skip(src, dest, true)) continue;
        do_link_dir(src, dest, true);
    }

    return report;
}


// Copy/link files from the profile repository back to system targets.
// Never throws; returns Report summarizing failures.
Report CM::repoToSystem()
{
    Report report;
    COMPTIME_STR ERR = "Skipping target: ";
    const fs::path repo_d = data_d / activeProf();

    bool have_sudo_targets = (
        !sudo_files_to_copy.empty() || !sudo_files_to_link.empty() ||
        !sudo_dirs_to_copy.empty()  || !sudo_dirs_to_link.empty()
    );
    if (have_sudo_targets) {
        core::print("This profile has @sudo entries - you may be asked for your password.\n");
        core::CmdStream{}.add("sudo -v").run(false, false, true);
    }

    auto apply_copy_file = [&](const fs::path& target, const fs::path& stored, bool use_sudo) {
        const fs::path src = repo_d / stored;
        std::error_code ec;
        if (!fs::exists(src, ec)) {
            core::print(ERR, "stored file missing: '", src.string(), "'\n");
            return;
        }
        core::ensure_directories(target.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo cp {} {}", core::shell_quote(src.string()), core::shell_quote(target.string()))
            .run(false, false, true);
            if (rc != 0) report.addComplain("sudo cp failed for '{}'", src.string());
            else core::print("  [sudo-copy] ", src.string(), " -> ", target.string(), "\n");
        } else {
            if (!core::copy_file_safe(src, target)) {
                report.addComplain("copy failed for '{}'", src.string());
            } else {
                core::print("  [copy] ", src.string(), " -> ", target.string(), "\n");
            }
        }
    };

    auto apply_link_file = [&](const fs::path& target, const fs::path& stored, bool use_sudo) {
        const fs::path src = repo_d / stored;
        std::error_code ec;
        if (!fs::exists(src, ec)) {
            core::print(ERR, "stored file missing: '", src.string(), "'\n");
            return;
        }
        core::ensure_directories(target.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo rm -f {}", core::shell_quote(target.string()))
                .add("sudo ln -s {} {}", core::shell_quote(src.string()), core::shell_quote(target.string()))
            .run(true, false, true);
            if (rc != 0) report.addComplain("sudo ln failed for '{}'", src.string());
            else core::print("  [sudo-link] ", src.string(), " -> ", target.string(), "\n");
        } else {
            if (fs::exists(target, ec) || fs::is_symlink(target, ec)) {
                fs::remove(target, ec);
            }
            fs::create_symlink(src, target, ec);
            if (ec) report.addComplain("symlink failed for '{}': {}", target.string(), ec.message());
            else core::print("  [link] ", src.string(), " -> ", target.string(), "\n");
        }
    };

    auto apply_copy_dir = [&](const fs::path& target, const fs::path& stored, bool use_sudo) {
        const fs::path src = repo_d / stored;
        std::error_code ec;
        if (!fs::exists(src, ec)) {
            core::print(ERR, "stored dir missing: '", src.string(), "'\n");
            return;
        }
        core::ensure_directories(target.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo cp -r {} {}", core::shell_quote(src.string()), core::shell_quote(target.string()))
            .run(false, false, true);
            if (rc != 0) report.addComplain("sudo cp -r failed for '{}'", src.string());
            else core::print("  [sudo-copy-dir] ", src.string(), " -> ", target.string(), "\n");
        } else {
            if (!core::copy_directory(src, target, false)) {
                report.addComplain("directory copy failed for '{}'", src.string());
            } else {
                core::print("  [copy-dir] ", src.string(), " -> ", target.string(), "\n");
            }
        }
    };

    auto apply_link_dir = [&](const fs::path& target, const fs::path& stored, bool use_sudo) {
        const fs::path src = repo_d / stored;
        std::error_code ec;
        if (!fs::exists(src, ec)) {
            core::print(ERR, "stored dir missing: '", src.string(), "'\n");
            return;
        }
        core::ensure_directories(target.parent_path());
        if (use_sudo) {
            int32 rc = core::CmdStream{}
                .add("sudo rm -rf {}", core::shell_quote(target.string()))
                .add("sudo ln -s {} {}", core::shell_quote(src.string()), core::shell_quote(target.string()))
            .run(true, false, true);
            if (rc != 0) report.addComplain("sudo ln -s (dir) failed for '{}'", src.string());
            else core::print("  [sudo-link-dir] ", src.string(), " -> ", target.string(), "\n");
        } else {
            if (fs::exists(target, ec) || fs::is_symlink(target, ec)) {
                fs::remove_all(target, ec);
            }
            fs::create_directory_symlink(src, target, ec);
            if (ec) report.addComplain("directory symlink failed for '{}': {}", target.string(), ec.message());
            else core::print("  [link-dir] ", src.string(), " -> ", target.string(), "\n");
        }
    };

    for (auto& [target, stored] : files_to_copy)  apply_copy_file(target, stored, false);
    for (auto& [target, stored] : files_to_link)  apply_link_file(target, stored, false);
    for (auto& [target, stored] : dirs_to_copy)   apply_copy_dir(target, stored, false);
    for (auto& [target, stored] : dirs_to_link)   apply_link_dir(target, stored, false);
    for (auto& [target, stored] : sudo_files_to_copy) apply_copy_file(target, stored, true);
    for (auto& [target, stored] : sudo_files_to_link) apply_link_file(target, stored, true);
    for (auto& [target, stored] : sudo_dirs_to_copy)  apply_copy_dir(target, stored, true);
    for (auto& [target, stored] : sudo_dirs_to_link)  apply_link_dir(target, stored, true);

    return report;
}



ConfigManager dotty;
