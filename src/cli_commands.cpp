#include "cli.hpp"
#include "CmdStream.hpp"

int32 CmdLine::do_init() {
    // Init only bootstraps local directories and an empty master config.
    // Profile creation is explicit via `dotty profile new`.

    if (dotty.HOME.empty()) {
        core::print(
            "[Error] HOME environment variable is not set.\n"
            "Cannot initialize dotty without a home directory.\n"
        );
        return EXIT_FAILURE;
    }

    // Soft dependency checks (warnings only - init itself does not need gh/internet)
    if (!core::os::in_path("gh")) {
        core::print(
            "[Warning] runtime-dependency '\033[31mgithub-cli\033[0m' is not installed.\n"
            "You will need it later for `dotty profile new` / push / pull.\n"
        );
    }
    if (!core::os::in_path(core::PPRINTER)) {
        core::print(
            "[Warning] optional dependency '\033[31m", core::PPRINTER, "\033[0m' is not installed.\n"
        );
    }

    if (!core::ensure_directories(dotty.config_d)) {
        core::print("[Error] Could not create config directory: ", dotty.config_d.string(), "\n");
        return EXIT_FAILURE;
    }
    if (!core::ensure_directories(dotty.data_d)) {
        core::print("[Error] Could not create data directory: ", dotty.data_d.string(), "\n");
        return EXIT_FAILURE;
    }

    const fs::path master = dotty.HOME / dotty.master_src;
    std::error_code ec;
    if (fs::exists(master, ec) && !core::is_file_empty(master)) {
        if (!core::ask_confirm(
            "dotty is already initialized (master config exists).\n"
            "Reset master config and re-initialize?",
            false
        )) {
            core::print("Init aborted - existing configuration kept.\n");
            return EXIT_FAILURE;
        }
        if (!core::empty_file(master)) {
            core::print("[Error] Could not reset master config: ", master.string(), "\n");
            return EXIT_FAILURE;
        }
        std::ofstream fo(master, std::ios::out | std::ios::trunc);
        if (!fo) {
            core::print("[Error] Could not rewrite master config: ", master.string(), "\n");
            return EXIT_FAILURE;
        }
        fo << "# dotty master configuration\n"
           << "active-profile = \"[NIL-PROFILE]\"\n"
           << "config-editor = \"\"\n"
           << "profile = []\n";
        core::print("Reset '", master.string(), "'\n");
    } else {
        // Write a minimal valid master skeleton so later parse does not fail.
        std::ofstream fo(master, std::ios::out | std::ios::trunc);
        if (!fo) {
            core::print("[Error] Could not create master config: ", master.string(), "\n");
            return EXIT_FAILURE;
        }
        fo << "# dotty master configuration\n"
           << "active-profile = \"[NIL-PROFILE]\"\n"
           << "config-editor = \"\"\n"
           << "profile = []\n";
        fo.close();
    }

    core::print("dotty initialized.\n");
    core::print("  config dir : ", dotty.config_d.string(), "\n");
    core::print("  data dir   : ", dotty.data_d.string(), "\n");
    core::print("  master cfg : ", master.string(), "\n");
    core::print("\nNext step - create a profile:\n");
    core::print("  dotty profile new --name main --repo my-dotfiles --commit-msg \"init\"\n");
    return EXIT_SUCCESS;
}



// int32 CmdLine::do_clean(strview option) {
//     // {master, config, storage} toggle bytes
//     bool c, s = false; c = s;

//     if (option == "all") {c=s=true; goto _remove;}
//     if (option.contains("config")) c = true;
//     if (option.contains("storage")) s = true;

//     _remove:
//     return dotty.cleanConfigs(c, s).printOnBad();
// }



int32 CmdLine::do_update()
{
    if (dotty.noProfilesExist()) {
        core::print("To update a profile you should first create a profile\n");
        core::print("  tip: dotty profile new --name main --repo my-dotfiles --commit-msg \"init\"\n");
        return EXIT_FAILURE;
    }
    if (dotty.activeProf() == Profile::NOT) {
        core::print("To update a profile you should first set an active profile\n");
        core::print("  tip: dotty profile switch <name>\n");
        return EXIT_FAILURE;
    }

    const fs::path conf_path = dotty.config_d / dotty.activeProf() / dotty.config_src;
    std::error_code ec;
    if (!fs::exists(conf_path, ec)) {
        core::print("[Error] Config file does not exist: ", conf_path.string(), "\n");
        return EXIT_FAILURE;
    }

    std::ifstream conf(conf_path, std::ios::in);
    if (!conf.is_open()) {
        core::print("[Error] Could not open config file: ", conf_path.string(), "\n");
        return EXIT_FAILURE;
    }

    // Lex whole file line-by-line, concatenate tokens, single parseMain so
    // directives on earlier lines enable actions on later lines.
    DotlangLexer lexer;
    DotlangParser parser;
    parser.resetFileState();

    std::vector<Token> all_tokens;
    std::string line;
    while (std::getline(conf, line)) {
        lexer.feed(line);
        Report lr = lexer.lexMain();
        lr.printComplains();
        auto& toks = lexer.result();
        all_tokens.insert(all_tokens.end(), toks.begin(), toks.end());
    }

    parser.feed(std::move(all_tokens));
    ParseReport pr = parser.parseMain();
    pr.printComplains();

    // Load parsed lists into ConfigManager
    dotty.files_to_copy      = std::move(parser.copy_files);
    dotty.files_to_link      = std::move(parser.link_files);
    dotty.dirs_to_copy       = std::move(parser.copy_dirs);
    dotty.dirs_to_link       = std::move(parser.link_dirs);
    dotty.sudo_files_to_copy = std::move(parser.sudo_copy_files);
    dotty.sudo_files_to_link = std::move(parser.sudo_link_files);
    dotty.sudo_dirs_to_copy  = std::move(parser.sudo_copy_dirs);
    dotty.sudo_dirs_to_link  = std::move(parser.sudo_link_dirs);
    dotty.exec_commands      = std::move(parser.exec_commands);

    Report apply = dotty.systemToRepo();
    apply.printOnBad();

    Report exec_r = dotty.runExecCommands();
    exec_r.printOnBad();

    if (apply.error() || exec_r.error()) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}



int32 CmdLine::do_push(const char* commit_message) {
    if (dotty.activeProf() == Profile::NOT) {
        core::print("To push a profile, first set an active profile\n");
        return EXIT_FAILURE;
    }
    if (!core::internet_is_connected()) {
        core::print("Push operation requires an internet connection\n");
        return EXIT_FAILURE;
    }

    const std::string active = dotty.activeProf();
    const fs::path cfg_src  = dotty.config_d / active;
    const fs::path data_dst = dotty.data_d / active / dotty.data_cfgref;
    const fs::path repo_d   = dotty.data_d / active;

    if (!core::ensure_directories(cfg_src)) {
        core::print("[Error] Could not ensure config directory: ", cfg_src.string(), "\n");
        return EXIT_FAILURE;
    }
    if (!core::ensure_directories(data_dst)) {
        core::print("[Error] Could not ensure data directory: ", data_dst.string(), "\n");
        return EXIT_FAILURE;
    }

    // Copy profile config sources into the local repo before commit
    if (!core::copy_directory_contents(cfg_src, data_dst)) {
        core::print("[Error] Failed to copy config into repo\n");
        return EXIT_FAILURE;
    }

    int32 add_rc = core::CmdStream {}
        .add("cd {}", core::shell_quote(repo_d.string()))
        .add("git add .")
    .run(true, false);
    if (add_rc != 0) {
        core::print("[Error] git add failed (exit ", add_rc, ")\n");
        return EXIT_FAILURE;
    }

    // Empty commits are not an error - still push whatever is on the branch.
    const char* msg = (commit_message && commit_message[0]) ? commit_message : "dotty update";
    int32 commit_rc = core::CmdStream {}
        .add("cd {}", core::shell_quote(repo_d.string()))
        .add("git commit -m {}", core::shell_quote(msg))
    .run(true, false);
    if (commit_rc != 0) {
        core::print("Nothing new to commit (or commit failed); continuing with push.\n");
    }

    int32 rc = core::CmdStream {}
        .add("cd {}", core::shell_quote(repo_d.string()))
        .add("git push")
    .run(true, false);

    if (rc != 0) {
        core::print("[Error] git push failed (exit ", rc, ")\n");
        return EXIT_FAILURE;
    }

    core::print("Pushed profile '", active, "' successfully.\n");
    return EXIT_SUCCESS;
}



int32 CmdLine::do_pull() {
    if (dotty.activeProf() == Profile::NOT || dotty.noProfilesExist()) {
        core::print("Pull operation requires an active profile to be set\n");
        return EXIT_FAILURE;
    }
    if (!core::internet_is_connected()) {
        core::print("Pull operation requires an internet connection\n");
        return EXIT_FAILURE;
    }

    if (!core::ask_confirm("You are about to overwrite your current profile files. Continue?")) {
        core::print("Pull aborted.\n");
        return EXIT_FAILURE;
    }

    const Profile* const active_prof = dotty.getProfileByName(dotty.activeProf());
    if (active_prof == nullptr) {
        core::print("[Error] Active profile record is missing from master config\n");
        return EXIT_FAILURE;
    }

    const std::string active_config_d = (dotty.config_d / active_prof->name).string();
    const std::string active_data_d   = (dotty.data_d / active_prof->name).string();
    const fs::path cache_root = dotty.HOME / ".cache" / "dotty";
    const fs::path cache_clone = cache_root / active_prof->name;

    if (!core::ensure_directories(dotty.config_d / active_prof->name)) {
        core::print("[Error] Could not create config directory\n");
        return EXIT_FAILURE;
    }
    if (!core::ensure_directories(dotty.data_d / active_prof->name / dotty.data_cfgref)) {
        core::print("[Error] Could not create data directory\n");
        return EXIT_FAILURE;
    }
    if (!core::ensure_directories(cache_root)) {
        core::print("[Error] Could not create cache directory: ", cache_root.string(), "\n");
        return EXIT_FAILURE;
    }

    // Remove previous cache clone if any
    core::remove_path(cache_clone);

    int32 clone_rc = core::CmdStream {}
        .add("cd {}", core::shell_quote(cache_root.string()))
        .add("git clone {} {}",
             core::shell_quote(active_prof->repo_url),
             core::shell_quote(active_prof->name))
    .run(true, false);

    if (clone_rc != 0) {
        core::print("[Error] git clone failed for '", active_prof->repo_url, "'\n");
        return EXIT_FAILURE;
    }

    // Replace local data dir with cloned content
    core::remove_path(dotty.data_d / active_prof->name);
    if (!core::ensure_directories(dotty.data_d / active_prof->name) ||
        !core::copy_directory_contents(cache_clone, dotty.data_d / active_prof->name)) {
        core::print("[Error] Failed to copy clone into data directory\n");
        return EXIT_FAILURE;
    }

    // Restore config references from .dotty.d back to the config directory
    std::error_code ec;
    const fs::path cfgref = dotty.data_d / active_prof->name / dotty.data_cfgref;
    if (fs::exists(cfgref, ec) && fs::is_directory(cfgref, ec)) {
        core::ensure_directories(active_config_d);
        if (!core::copy_directory_contents(cfgref, fs::path(active_config_d))) {
            core::print("[Warning] Failed to restore config reference directory\n");
        }
        core::remove_path(cfgref);
    }

    // Re-parse the restored config so mappings/exec are available for repoToSystem
    {
        const fs::path conf_path = dotty.config_d / active_prof->name / dotty.config_src;
        std::ifstream conf(conf_path, std::ios::in);
        if (conf.is_open()) {
            DotlangLexer lexer;
            DotlangParser parser;
            parser.resetFileState();
            std::vector<Token> all_tokens;
            std::string line;
            while (std::getline(conf, line)) {
                lexer.feed(line);
                lexer.lexMain().printComplains();
                auto& toks = lexer.result();
                all_tokens.insert(all_tokens.end(), toks.begin(), toks.end());
            }
            parser.feed(std::move(all_tokens));
            parser.parseMain().printComplains();

            dotty.files_to_copy      = std::move(parser.copy_files);
            dotty.files_to_link      = std::move(parser.link_files);
            dotty.dirs_to_copy       = std::move(parser.copy_dirs);
            dotty.dirs_to_link       = std::move(parser.link_dirs);
            dotty.sudo_files_to_copy = std::move(parser.sudo_copy_files);
            dotty.sudo_files_to_link = std::move(parser.sudo_link_files);
            dotty.sudo_dirs_to_copy  = std::move(parser.sudo_copy_dirs);
            dotty.sudo_dirs_to_link  = std::move(parser.sudo_link_dirs);
            dotty.exec_commands      = std::move(parser.exec_commands);
        }
    }

    Report apply = dotty.repoToSystem();
    apply.printOnBad();

    Report exec_r = dotty.runExecCommands();
    exec_r.printOnBad();

    // Keep a config reference mirror inside data for future pushes
    core::ensure_directories(fs::path(active_data_d) / dotty.data_cfgref);
    core::copy_directory_contents(fs::path(active_config_d), fs::path(active_data_d) / dotty.data_cfgref);

    if (apply.error() || exec_r.error()) return EXIT_FAILURE;
    core::print("Pulled and applied profile '", active_prof->name, "' successfully.\n");
    return EXIT_SUCCESS;
}


// note: editor name is then passed to `which` command
int32 CmdLine::do_config(strview what_cfg, const strview editor_name) {
    // prompt editing suggestion, and edit is accepted
    auto suggest_edit = [editor_name](const fs::path cfg_path)->int32 {
        if (!core::ask_confirm("Do you want to edit this file?")) {
            return EXIT_FAILURE;
        }
        else {
            std::string editor;
            if (!editor_name.empty()) {
                core::CmdStream cmd;
                cmd.add("which {}", editor_name).run(false, true, false);
                editor = cmd.output();
            } else {
                MasterConfigParser mcp;
                mcp.rParse(dotty.HOME/dotty.master_src).printComplains();
                mcp.rEval().printComplains();
                editor = mcp.vars[mcp.P_CFG_EDITOR];
            }
            if (editor.empty()) {
                editor = core::os::get_txt_editor();
            }
            if (editor.empty()) {
                core::print("[Error] No editor found. Set EDITOR or pass -e.\n");
                return EXIT_FAILURE;
            }
            return core::CmdStream {}
                .add("{} {}", editor, cfg_path.string())
            .run(false, false, false);
        }
    };

    dotty.reloadConfig().printComplains();

    // default to master if no profiles exist or active profile is not set
    if (dotty.noProfilesExist()) {
        return suggest_edit(dotty.HOME/dotty.master_src);
    }
    else if (dotty.activeProf() == Profile::NOT) {
        return suggest_edit(dotty.HOME/dotty.master_src);
    }
    else
    {
        // Default option
        if (what_cfg == "") {
            fs::path config_source = dotty.config_d/dotty.activeProf()/dotty.config_src;
            if (!fs::exists(config_source)) {
                core::new_file(config_source);
            }
            bool pprinted = core::pprint_file(config_source);
            if (!pprinted) core::print(
                "[Failed to pretty-print file, ", core::PPRINTER, " probably doesn't exist!]\n"
            );

            return suggest_edit(dotty.config_d/dotty.activeProf()/dotty.config_src);
        }
        // Master option
        else if (what_cfg == "master") {
            if (!fs::exists(dotty.HOME/dotty.master_src)) {
                core::print("Can't find master configuration: File doesn't exist!\n");
                return EXIT_FAILURE;
            }

            bool pprinted = core::pprint_file(dotty.HOME/dotty.master_src);
            if (!pprinted) core::print(
                "[Failed to pretty-print file, ", core::PPRINTER, " probably doesn't exist!]\n"
            );
            return suggest_edit(dotty.HOME/dotty.master_src);
        }
        // Unknown option
        else
        {
            core::print("config: Unknown flag '", what_cfg, "'\n");
            return EXIT_FAILURE;
        }
    }
}



// This is a subcommand with subcommands(naming convention: do_<subc>_)
int32 CmdLine::do_profile_(strview option) {
    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_list(const strview options/*= "name,repo,url,gh"*/) {
    if (dotty.noProfilesExist()) {
        core::print("No profiles exist yet!\n");
        return EXIT_FAILURE;
    }

    bool name, repo, url, gh = false; name = repo = url = gh;
    if (options == "all") {name=repo=url=gh=true; goto _print;}
    if (options.contains("name")) name = true;
    if (options.contains("repo")) repo = true;
    if (options.contains("url"))  url = true;
    if (options.contains("gh"))   gh = true;

_print:
    core::print("    [ PROFILES ]\n");
    dotty.listProfiles(name, repo, url, gh).mute();
    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_new(
    const std::string& name, const std::string& repo_name,
    bool pub, const std::string& commit_msg
){
    std::optional<std::string> gh_acc = core::active_github_account();
    if (!gh_acc.has_value()) {
        core::print("[Error] GitHub login not found. Run 'gh auth login' first.\n");
        return EXIT_FAILURE;
    }

    Report r = dotty.newProfile(
        name, gh_acc.value(), repo_name, pub,
        false, commit_msg.c_str()
    );
    if (r.error()) {
        r.printOnBad();
        return EXIT_FAILURE;
    }

    core::print("Created profile '", name, "' and set it active.\n");
    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_delete(const std::string& profile_name) {
    if (dotty.noProfilesExist()) {
        core::print("Can't delete a profile: no profiles exist yet!\n");
        return EXIT_FAILURE;
    }
    const Profile* prof = dotty.getProfileByName(profile_name);
    if (prof == nullptr) {
        core::print(
            "Could not delete '", profile_name,
            "': profile does not exist!\n"
        );
        return EXIT_FAILURE;
    }

    const std::string repo_url = prof->repo_url;
    const std::string owner_repo = core::owner_repo_from_url(repo_url);
    const bool was_active = (dotty.activeProf() == profile_name);

    if (!core::ask_confirm(
        std::format(
            "Delete profile '{}'?\n"
            "  This removes local config/data and (if possible) GitHub repo '{}'.",
            profile_name, owner_repo
        ),
        false
    )) {
        core::print("Delete aborted.\n");
        return EXIT_FAILURE;
    }

    // 1. Update master config FIRST so a crash later does not leave a ghost profile
    //    whose directories are already gone.
    Report del = dotty.deleteProfile(profile_name);
    if (del.error()) {
        del.printOnBad();
        return EXIT_FAILURE;
    }

    // 2. Remove local directories
    core::print("Deleting profile files and directories...\n");
    std::error_code fs_err;
    fs::remove_all(dotty.config_d / profile_name, fs_err);
    if (fs_err) {
        core::print("[Warning] Could not fully remove config dir: ", fs_err.message(), "\n");
    }
    fs_err.clear();
    fs::remove_all(dotty.data_d / profile_name, fs_err);
    if (fs_err) {
        core::print("[Warning] Could not fully remove data dir: ", fs_err.message(), "\n");
    }

    // 3. Delete GitHub repo (non-fatal if it fails)
    if (owner_repo != "[BAD-URL]" && core::os::in_path("gh") && core::internet_is_connected()) {
        int32 repo_deletion_failed = core::CmdStream{}
            .add("gh repo delete {} --yes", core::shell_quote(owner_repo))
        .run(false, false, true);

        if (repo_deletion_failed) {
            core::print(
                "[Warning] Couldn't delete GitHub repo '", owner_repo,
                "'. Local profile was still removed.\n"
            );
        }
    }

    if (was_active) {
        core::print("Active profile was deleted. Switch to another with: dotty profile switch <name>\n");
    }
    core::print("Deleted profile '", profile_name, "'.\n");
    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_switch(const std::string& profile_name) {
    Report fault = dotty.setActiveProfile(profile_name);
    if (fault.error()) {
        fault.printOnBad();
        return EXIT_FAILURE;
    }
    core::print("Active profile: ", profile_name, "\n");
    return EXIT_SUCCESS;
}
