#include "cli.hpp"

int32 CmdLine::do_init() {
    if (!cm::is_file_empty(dotty.HOME/dotty.master_src)) {
        if (!cm::ask_confirm(
            "You already have initialized dotty once in yout system!\n"
            "Wanna reset it and continue do initialize it again?",
            false
        )) {
            cm::terminate("You can't run master config preconfiguired!\n");
        }
        else {
            cm::empty_file(dotty.HOME/dotty.master_src);
            cm::print("Resetted '", dotty.HOME/dotty.master_src, "'\n");
        }
    }

    // Check if internet is connected
    if (!cm::internet_is_connected()) {
        cm::terminate(
            "Your device is not connected to the internet!\n",
            "Repository creation requires a wifi connection."
        );
    }

    if (!cm::os::in_path("gh")) {
        cm::terminate(
            "[Error] core runtime-dependency '", "\033[31mgithub-cli\033[0m", "' doesn't exist!\n",
            "You can probably install it with you package manager"
        );
    }
    if (!cm::os::in_path(cm::PPRINTER)) {
        cm::print(
            "[Warning] runtime-dependency '\033[31m", cm::PPRINTER, "\033[0m' doesn't exist!\n",
            "You can install it with you package manager or by manually"
        );
    }

    // Create directories if not exist
    cm::ensure_directories(dotty.config_d);
    cm::ensure_directories(dotty.data_d);

    // Check github authentication status
    cm::debug("Checking GitHub CLI authentication...\n");
    int32 auth_failed = ::system("gh auth status --hostname github.com >" NULLDEV);
    if (auth_failed) {
        cm::terminate("gh is not authenticated. Please run 'gh auth login' first.\n");
    } else {
        cm::debug("GitHub CLI authenticated.\n");
    }

    // Get github authenticated username
    auto get_gh_auth = cm::CmdStream {}
        .add("gh api user --jq '.login'")
    ;
    if (get_gh_auth.run(" && ", true) != 0) {
        cm::terminate("Fetching authenticated github username failed!");
    }
    const std::string& gh_auth_name = get_gh_auth.output();


    // ASK PROFILE-NAME
    static const std::string ini_prof_default = "main";
    std::string ini_prof;
    cm::prompt(std::format("Enter profile name[{}]: ", ini_prof_default).c_str(), ini_prof);
    if (ini_prof.empty()) ini_prof = "main";
    dotty.validateProfileName(ini_prof).printOnBad().terminateOnBad();

    // ASK REPO-NAME
    std::string repo_name;
    cm::prompt("Enter a name for your dotty config repo: ", repo_name);
    dotty.validateRepoName(repo_name).printOnBad().terminateOnBad();
    const std::string repo_url = cm::make_repo_url(gh_auth_name, repo_name);
    cm::debug("URL constructed: ", repo_url);

    // ASK REPO-VISIBILITY
    int32 vis_inp;
    cm::prompt_number("Enter repo visibility (1: private, 2: public) [1]: ", vis_inp);
    if (!cm::is_any_of(vis_inp, {1, 2})) {
        cm::print("Invalid input. Defaulting to private.\n\n");
        vis_inp = 1;
    } else cm::print("\n");

    // ASK COMMIT-MESSAGE
    static const std::string default_commit_msg = "\"Initial commit of this configuration profile\"";
    std::string commit_msg;
    cm::prompt(std::format("Enter commit message [{}]: ", default_commit_msg).c_str(), commit_msg);
    if (commit_msg.empty()) commit_msg = default_commit_msg;

    // CREATE NEW PROFILE
    dotty.newProfile(ini_prof, gh_auth_name, repo_name, bool(vis_inp-1), false, commit_msg.c_str())
        .printOnBad()
        .terminateOnBad();

    cm::print("Repo '", repo_name, "' created as ", (vis_inp-1)?("public"):("private"), " on GitHub.\n");
    cm::print("Setting ", ini_prof, " active profile\n");
    dotty.setActiveProfile(ini_prof).printOnBad();
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



int32 CmdLine::do_update() {
    if (dotty.noProfilesExist()) {
        cm::print("To update a profile you should first have to create a profile\n");
        return EXIT_FAILURE;
    }
    if (dotty.activeProf() == Profile::NOT) {
        cm::print("To update a profile you should first have to have a active profile set\n");
        return EXIT_FAILURE;
    }

    Lexer lexer;
    ConfigParser parser;
    std::ifstream conf(
        dotty.config_d/dotty.activeProf()/dotty.config_src,
        std::ios::in
    );
    if (!conf.is_open()) cm::terminate("File could not be opened!\n");

    std::string line;
    while (std::getline(conf, line)) {
        cm::debug("Lexing config...\n");
        lexer.feed(line);
        lexer.lexMain().printComplains();
#if DEBUG_ON
        cm::debug("\n\nLexed tokens:\n");
        lexer.print();
#endif
        parser.feed(lexer.result());
        cm::debug("Parsing tokens...\n");
        parser.parseMain().printComplains();

        cm::debug("Loading parsed lists...\n\n");
        dotty.files_to_copy = parser.copy_files;
        dotty.files_to_link = parser.link_files;
        dotty.dirs_to_copy = parser.copy_dirs;
        dotty.dirs_to_link = parser.link_dirs;
    }
    enum { CPF, LNF, CPD, LND };  // CP=copy, LN=link, F=file, D=dir
    // load succeed ones
    auto succeed = dotty.systemToRepo();
    auto copied_files = succeed[CPF];
    auto linked_files = succeed[LNF];
    auto copied_dirs = succeed[CPD];
    auto linked_dirs = succeed[LND];

    if (!copied_files.empty()) {
        cm::print("Copied files:\n");
        for (auto& [src, dest]  : copied_files) {
            cm::print(
                "  '", src.string(), "' -> '", dest.string(), "'\n"
            );
        }
    } else cm::print("No files copied!\n");
    if (!copied_dirs.empty()) {
        cm::print("Copied directories:\n");
        for (auto& [src, dest]  : copied_dirs) {
            cm::print(
                "  '", src.string(), "' -> '", dest.string(), "'\n"
            );
        }
    } else cm::print("No directories copied!\n");
    if (!linked_files.empty()) {
        cm::print("Linked files:\n");
        for (auto& [src, dest]  : linked_files) {
            cm::print(
                "  '", src.string(), "' -> '", dest.string(), "'\n"
            );
        }
    } else cm::print("No files linked!\n");
    if (!linked_dirs.empty()) {
        cm::print("Linked directories:\n");
        for (auto& [src, dest]  : linked_dirs) {
            cm::print(
                "  '", src.string(), "' -> '", dest.string(), "'\n"
            );
        }
    } else cm::print("No directories linked!\n");

    return EXIT_SUCCESS;
}



int32 CmdLine::do_push(const char* commit_message) {
    if (dotty.activeProf() == Profile::NOT) {
        cm::print("To push a profile, first you have to set active profile");
        return EXIT_FAILURE;
    }
    if (!cm::internet_is_connected()) {
        cm::print("Pull operation requires internet connection\n");
        return EXIT_FAILURE;
    }

    cm::ensure_directories(dotty.config_d / dotty.activeProf());
    cm::ensure_directories(dotty.data_d / dotty.activeProf() / dotty.data_cfgref);
    // Copy all config source and includes to local repo(config storage) before push
    fs::copy(
        dotty.config_d / dotty.activeProf(),
        dotty.data_d / dotty.activeProf() / dotty.data_cfgref,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing
    );

    cm::CmdStream cmd;
    cmd
        .add("cd {}", (dotty.data_d/dotty.activeProf()).string())
        .add("git add .")
        .add("git commit -m \"{}\"", commit_message)
        .add("git push")
    .run(" && ", false);

    return EXIT_SUCCESS;
}



int32 CmdLine::do_pull() {
    if (dotty.activeProf()==Profile::NOT || dotty.noProfilesExist()) {
        cm::print("Pull operation requires active profile to be set\n");
        return EXIT_FAILURE;
    }
    if (!cm::internet_is_connected()) {
        cm::print("Pull operation requires internet connection\n");
        return EXIT_FAILURE;
    }

    if (!cm::ask_confirm("You are about to overwrite your current profile, are you are sure?")) {
        cm::terminate("Pulling aborted.");
    }

    const Profile* const active_prof = dotty.getProfileByName(dotty.activeProf());
    std::string active_config_d = (dotty.config_d/active_prof->name).string();
    std::string active_data_d = (dotty.data_d/active_prof->name).string();

    cm::ensure_directories(dotty.config_d / dotty.activeProf() / dotty.data_cfgref);
    cm::ensure_directories(dotty.data_d / dotty.activeProf() / dotty.data_cfgref);
    cm::ensure_directories(dotty.HOME/".cache/dotty/");
    cm::CmdStream {}
        .add("cd $HOME/.cache/dotty/")
        .add("rm -rf ./{}", active_prof->name)
        .add("git clone {} {}", active_prof->repo_url, active_prof->name)
        .add("rm -rf {}/*", active_config_d)
    .run(" && ", false);

    // Copy cache-dir to data directory
    fs::copy(
        dotty.HOME / ".cache" / "dotty" / active_prof->name,
        dotty.data_d,
        fs::copy_options::overwrite_existing
    );

    // Copy all storage config references back to dotty config directory
    for (auto& item  : fs::directory_iterator(dotty.data_d/dotty.activeProf())) {
        const std::string item_name = item.path().filename();
        if (item.is_directory() && (item_name == dotty.data_cfgref)) {
            fs::copy(
                item, active_config_d,
                fs::copy_options::overwrite_existing
            );
            fs::remove_all(item);
        }
    }
    dotty.repoToSystem();
    fs::copy(
        active_config_d, fs::path(active_data_d)/dotty.data_cfgref,
        fs::copy_options::none
    );

    return EXIT_SUCCESS;
}


// note: editor name is then passed to `which` command
int32 CmdLine::do_config(strview what_cfg, const strview editor_name) {
    // prompt editing suggestion, and edit is accepted
    auto suggest_edit = [editor_name](const fs::path cfg_path)->int32 {
        if (!cm::ask_confirm("Do you want to edit this file?")) {
            return EXIT_FAILURE;
        }
        else {
            std::string editor;
            if (!editor_name.empty()) {
                cm::CmdStream cmd;
                cmd.add("which {}", editor_name).run("", true);
                editor = cmd.output();
            } else {
                MasterConfigParser mcp;
                mcp.rParse(dotty.HOME/dotty.master_src).printComplains();
                mcp.rEval().printComplains();
                editor = mcp.vars[mcp.P_CFG_EDITOR];
            }
            return cm::CmdStream{}
                .add("{} {}", editor, cfg_path.string())
                .run(" && ", false);
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
                cm::new_file(config_source);
            }
            bool pprinted = cm::pprint_file(config_source);
            if (!pprinted) cm::print(
                "[Failed to pretty-print file, ", cm::PPRINTER, " probably doesn't exist!]\n"
            );

            return suggest_edit(dotty.config_d/dotty.activeProf()/dotty.config_src);
        }
        // Master option
        else if (what_cfg == "master") {
            if (!fs::exists(dotty.HOME/dotty.master_src)) {
                cm::print("Can't find master configuration: File doesn't exist!\n");
                return EXIT_FAILURE;
            }

            bool pprinted = cm::pprint_file(dotty.HOME/dotty.master_src);
            if (!pprinted) cm::print(
                "[Failed to pretty-print file, ", cm::PPRINTER, " probably doesn't exist!]\n"
            );
            return suggest_edit(dotty.HOME/dotty.master_src);
        }
        // Unknown option
        else
        {
            cm::print("config: Unknown flag '", what_cfg, "'\n");
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
        cm::print("No profiles exist yet!\n");
        return EXIT_FAILURE;
    }

    bool name, repo, url, gh = false; name = repo = url = gh;
    if (options == "all") {name=repo=url=gh=true; goto _print;}
    if (options.contains("name")) name = true;
    if (options.contains("repo")) repo = true;
    if (options.contains("url"))  url = true;
    if (options.contains("gh"))   gh = true;

_print:
    cm::print("    [ PROFILES ]\n");
    dotty.listProfiles(name, repo, url, gh).mute();
    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_new(
    const std::string& name, const std::string& repo_name,
    bool pub, const std::string& commit_msg
){
    std::optional<std::string> gh_acc = cm::active_github_account();
    if (!gh_acc.has_value()) cm::terminate("Github login not found");

    dotty.newProfile(
        name, cm::active_github_account().value(), repo_name, pub,
        false, commit_msg.data()
    ).printOnBad().terminateOnBad();

    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_delete(const std::string& profile_name) {
    if (dotty.noProfilesExist()) {
        cm::print("Can't delete a profile: no profiles exist yet!\n");
        return EXIT_FAILURE;
    }
    if (dotty.getProfileByName(profile_name) == nullptr) {
        cm::print(
            "Could not delete '", profile_name,
            "': profile does not exist!\n"
        );
        return EXIT_FAILURE;
    }

    // delete github repo
    int32 repo_deletion_failed =  cm::CmdStream{}
        .add("gh repo delete {}", cm::repo_from_url(dotty.getProfileByName(profile_name)->repo_url))
    .run(" && ", false);

    if (repo_deletion_failed) {
        cm::print("Couldn't delete github repo!\n");
        if (!cm::ask_confirm("Do you want to proceed with removing directories of it anyway?")) {
            cm::terminate("Could not remove '", profile_name, "'!");
        }
    }


    cm::print("Deleting profile files and directories!...\n");

    std::error_code res = {};
    //
    fs::remove_all(dotty.config_d/profile_name, res);
    if (res.value() != 0) {
         cm::print("[ERROR]: No profile config removed!\n");
    }
    //
    fs::remove_all(dotty.data_d/profile_name, res);
    if (res.value() != 0) {
        cm::print("[ERROR]: No profile storage data removed!\n");
    }

    dotty.deleteProfile(profile_name).printOnBad();

    return EXIT_SUCCESS;
}



int32 CmdLine::do_p_switch(const std::string& profile_name) {
    Report fault = dotty.setActiveProfile(profile_name);
    if (fault) {
        fault.printOnBad();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
