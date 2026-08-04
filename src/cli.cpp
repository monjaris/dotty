#include <CLI/CLI.hpp>
#include "cli.hpp"

struct CmdLine::Impl {
    CLI::App cli {"Config Manager CLI tool"};

    int argc; char** argv;

    const char* const default_msg = {
        "dotty: pass --help to show usage\n"
    };


    // `prof_agnostic` set to `true` means that this subcommand doesn't require an active profile
    struct CmdEntry { std::function<int32()> action; bool prof_agnostic=true; };
    std::map<CLI::App*, CmdEntry> sub_commands;

    // cache values
    struct {
        // strview clean_configs;
        std::string push_commit_msg    = {};
        strview config_what            = {};
            strview config_editor      = {};
        std::string list_properties    = {"all"};
        std::string new_prof_name      = {};
            std::string new_repo_name  = {};
            bool new_repo_pub          = {};
            std::string new_commit_msg = {};
        std::string delete_profile     = {};
        std::string switch_profile     = {};
    } v;
};

#define APP (impl->cli)



CmdLine::CmdLine(int argc, char** argv) : impl(new Impl()) {
    impl->argc = argc;
    impl->argv = impl->cli.ensure_utf8(argv);
}

CmdLine::~CmdLine() {delete impl;}


CLI::App* CmdLine::newSubCmd(
    CLI::App* parent, const inilist<const char*> names, const std::function<int32()>& fn,
    const char* desc, bool profile_agnostic, const int32 (&opt_min_max)[2], const int32 (&sc_min_max)[2]
) {
    if (names.size()>3 || names.size()<1) {
        core::terminate(
            "", __func__, ": Should pass minimum 1 and maximum 3 names/aliases."
        );
    }

    auto* sub_cmd = parent->add_subcommand(names.begin()[0], desc);
    if (names.size()>1 && names.begin()[1]) {
        sub_cmd->alias(names.begin()[1]);
    }
    if (names.size()>2 && names.begin()[2]) {
        sub_cmd->alias(names.begin()[2]);
    }

    sub_cmd->require_option(opt_min_max[0], opt_min_max[1]);
    sub_cmd->require_subcommand(sc_min_max[0], sc_min_max[1]);

    impl->sub_commands.emplace(sub_cmd, CmdLine::Impl::CmdEntry{fn, profile_agnostic});
    return sub_cmd;
}


#define BIND(_fn_name) [this](){return _fn_name;}
int32 CmdLine::setup()
{
    using SubCmd = CLI::App;

    SubCmd* sc_init = newSubCmd(&APP, {"init"},
        BIND(do_init()),
        "Initialize dotty config manager in your system", true, {0,0}, {0,0}
    );
    //
    SubCmd* sc_update = newSubCmd(&APP, {"update", "u"},
        BIND(do_update()),
        "Write configs to configs storage", false, {0,0}, {0,0}
    );
    //
    SubCmd* sc_push = newSubCmd(&APP, {"push"},
        BIND(do_push(impl->v.push_commit_msg.c_str())),
        "Push config storage to the github repo", false, {0,0}, {0,0}
    ); sc_push
        ->add_option("--commit-message", impl->v.push_commit_msg, "Push with commit message")->required()
    ;
    //
    SubCmd* sc_pull = newSubCmd(&APP, {"pull"},
        BIND(do_pull()),
        "Pull your config from the repo", false, {0,0}, {0,0}
    );
    //
    SubCmd* sc_config = newSubCmd(&APP, {"config", "c"},
        BIND(do_config(impl->v.config_what, impl->v.config_editor)),
        "Configuration utilities",false, {0,0}, {0,0}
    );
        sc_config->add_option("configuration", impl->v.config_what, "Which configuration to view/edit");
        sc_config->add_option("-e,--editor", impl->v.config_editor, "Configuire with chosen editor");
    ;
    //
    SubCmd* sc_profile_ = newSubCmd(&APP, {"profile", "p"},
        BIND(do_profile_("")),
        "Profile related commands", true, {0,0}, {1, 1}
    );
    //
        SubCmd* ssc_list = newSubCmd(sc_profile_, {"list", "l"},
            BIND(do_p_list(impl->v.list_properties)),
            "List all existing profiles", true, {0,1}, {0,0}
        ); ssc_list
            ->add_option("properties", impl->v.list_properties, "List profiles and opted properties")->default_val("name,url")
        ;
    //
        SubCmd* ssc_new = newSubCmd(sc_profile_, {"new", "n"},
            BIND(do_p_new(impl->v.new_prof_name, impl->v.new_repo_name, impl->v.new_repo_pub, impl->v.new_commit_msg)),
            "Create a new profile", true, {3,4}, {0,0}
        );
            ssc_new->add_option("name,--name,-n", impl->v.new_prof_name, "New profile's name")->required();
            ssc_new->add_option("--repo,-r", impl->v.new_repo_name, "New profile's repo name")->required();
            ssc_new->add_option("--commit-msg,-m", impl->v.new_commit_msg, "New profile's initial commit message")->required();
            ssc_new->add_flag("--public", impl->v.new_repo_pub, "New repo's publicity status");
        ;
    //
        SubCmd* ssc_delete = newSubCmd(sc_profile_, {"delete", "d"},
            BIND(do_p_delete(impl->v.delete_profile)),
            "Delete a profile", true, {1,1}, {0,0}
        ); ssc_delete
            ->add_option("profile", impl->v.delete_profile, "Name of the profile to delete")->required()
        ;
    //
        SubCmd* ssc_switch = newSubCmd(sc_profile_, {"switch", "s"},
            BIND(do_p_switch(impl->v.switch_profile)),
            "Switch to another profile", true, {1,1}, {0,0}
        ); ssc_switch
            ->add_option("profile", impl->v.switch_profile, "Name of the profile to switch")->required()
        ;

    //
    // SC sc_clean = newSubCmd("clean", BIND(do_clean(impl->v.clean_configs)), "Clean all configs for current profile", 1);
    //     sc_clean->add_option("config", impl->v.clean_configs, "Clean configs");
    //

    try {
        APP.parse(impl->argc, impl->argv);
    } catch (const CLI::ParseError& e) {
        ::exit(APP.exit(e));
    }

    return EXIT_SUCCESS;
}
#undef BIND


int32 CmdLine::run()
{
    if (::geteuid() == (uid_t) 0) {
        core::terminate<EXIT_SUCCESS>(
            "Running dotty with sudo is not a good practice!\n"
            "Dotty only asks for sudo if per-profile configuration"
        );
    }

    std::string active_p = Profile::NOT;
    dotty.load(true);
    active_p = dotty.activeProf();

    if (APP.count_all() == 1) {
        core::print(impl->default_msg);
        if (active_p == Profile::NOT) {
            core::print("Active profile is not set.\n");
        } else {
            core::print("Active-profile: ", "\033[32m", active_p, "\033[0m", "\n");
        }
    }

    for (auto& [sub_cmd, data] : impl->sub_commands) {
        if (
            sub_cmd->parsed() && !sub_cmd->get_subcommands([](CLI::App* sc){
                return sc->parsed();
            }).size()
        ){
            if (!data.prof_agnostic && (active_p == Profile::NOT)) {
                core::terminate("", sub_cmd->get_name(), " command requires an active profile!\n");
            }

            data.action.operator()();
        }
    }

    return EXIT_SUCCESS;
}

