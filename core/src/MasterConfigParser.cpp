#define TOML_HEADER_ONLY 0
#define TOML_IMPLEMENTATION
#include "toml++/toml.hpp"
#include "MasterConfigParser.hpp"

using MCP = MasterConfigParser;


struct MCP::Pimpl_Toml {
    toml::table table;
};


MCP::MasterConfigParser ()
    : m_toml(new Pimpl_Toml())
{}

MCP::~MasterConfigParser () {
    delete m_toml;
}


Report MCP::rParse(const fs::path& path)
{
    try {
        m_toml->table = toml::parse_file(path.string());
    } catch (const toml::parse_error& e) {
        return Report::Bad("Master config parse-error: {}", e.description());
    }
    return Report::Good();
}


Report MCP::rEval()
{
    Report rep;
    profiles.clear();

    vars[P_ACTIVE_PROF] = m_toml->table[P_ACTIVE_PROF].value_or(std::string{Profile::NOT});
    vars[P_CFG_EDITOR]  = m_toml->table[P_CFG_EDITOR].value_or(
        std::string{core::os::get_txt_editor()}
    );

    auto* arr_profiles = m_toml->table[P_PROFILES].as_array();
    // Empty or missing profiles array is valid before a first profile exists.
    if (!arr_profiles) {
        return Report::Good();
    }

    for (auto& node_profile  : *arr_profiles) {
        toml::table* tbl_profile = node_profile.as_table();
        if (!tbl_profile || tbl_profile->empty()) {
            rep.addComplain("Profile table is empty! ignoring..");
            continue;
        }
        toml::table& prof = *tbl_profile;

        std::string name = prof[PP_NAME].value_or(std::string{Profile::NOT});
        std::string url  = prof[PP_REPO_URL].value_or(std::string{});
        // Default missing `public` to false rather than skipping the whole profile.
        bool is_public   = prof[PP_REPO_PUB].value_or(false);
        bool is_extern   = prof[PP_EXTERNAL].value_or(false);

        if (name.empty() || name == Profile::NOT || url.empty()) {
            rep.addComplain("Profile have missing properties, skipping..");
            continue;
        }
        profiles.push_back(Profile{name, url, is_public, is_extern});
    }

    return rep;
}


Report MCP::rValidateConfig() {
    Report report;
    for (auto& prof_1  : profiles) {
        for (auto& prof_2  : profiles) {
            if (&prof_1 == &prof_2) continue;  // skip same object
            if (prof_1.name == prof_2.name) {  // check if they have same name
                report.addComplain("Duplicate-profile: {}", prof_1.name);
            }
        }
    }
    return report.error()? report.Bad("\nBad config!") : Report::Good();
}


Report MCP::wActivateProfile(const strview name) {
    // Updating an existing active-profile key is the normal path, not an error.
    m_toml->table.insert_or_assign(P_ACTIVE_PROF, std::string{name});
    return Report::Good();
}


Report MCP::wSetDefaultEditor(const strview editor) {
    m_toml->table.insert_or_assign(P_CFG_EDITOR, std::string{editor});
    return Report::Good();
}

Report MCP::wAddProfile(const Profile& prof) {
    toml::table entry;
    entry.insert(PP_NAME, prof.name);
    entry.insert(PP_REPO_PUB, prof.is_pub);
    entry.insert(PP_EXTERNAL, prof.is_ext);
    entry.insert(PP_REPO_URL, prof.repo_url);

    auto* arr = m_toml->table[P_PROFILES].as_array();
    if (!arr) {
        m_toml->table.insert_or_assign(P_PROFILES, toml::array{});
        arr = m_toml->table[P_PROFILES].as_array();
        if (!arr) return Report::Bad("Could not create profiles array in master config");
    }

    for (auto& node : *arr) {
        auto* tbl = node.as_table();
        if (!tbl) continue;
        if ((*tbl)[PP_NAME].value_or(std::string{}) == prof.name) {
            return Report::Bad("Profile '{}' already exists in master config", prof.name);
        }
    }

    arr->push_back(std::move(entry));
    return Report::Good();
}


Report MCP::wRemoveProfile(const strview name) {
    auto* arr_profiles = m_toml->table[P_PROFILES].as_array();
    if (arr_profiles == nullptr) {
        return Report::Bad("Profiles array is empty");
    }

    const std::string want{name};
    for (uint32 i = 0; i < arr_profiles->size(); ++i) {
        toml::table* tbl = arr_profiles->at(i).as_table();
        if (!tbl) continue;
        const std::string have = (*tbl)[PP_NAME].value_or(std::string{});
        if (have == want) {
            auto it = arr_profiles->begin();
            std::advance(it, static_cast<std::ptrdiff_t>(i));
            arr_profiles->erase(it);
            return Report::Good();
        }
    }

    return Report::Bad("Profile '{}' doesn't exist!", name);
}


Report MCP::wSaveConfig(const fs::path& path) {
    std::ofstream fo(path, std::ios::out | std::ios::trunc);
    if (!fo) return Report::Bad("Couldn't open output file: '{}'", path.string());
    fo << m_toml->table;
    fo.flush();
    if (!fo) return Report::Bad("Couldn't write master config: '{}'", path.string());
    return Report::Good();
}
