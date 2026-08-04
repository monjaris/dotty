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

    vars[P_ACTIVE_PROF] = m_toml->table[P_ACTIVE_PROF].value_or(Profile::NOT);
    vars[P_CFG_EDITOR]  = m_toml->table[P_CFG_EDITOR].value_or(core::os::get_txt_editor());

    auto* arr_profiles = m_toml->table[P_PROFILES].as_array();
    if (!arr_profiles) return rep.Bad("No profiles configuired!");

    for (auto& node_profile  : *arr_profiles) {
        toml::table* tbl_profile = node_profile.as_table();
        if (!tbl_profile || tbl_profile->empty()) {
            rep.addComplain("Profile table is empty! ignoring..");
            continue;
        }
        toml::table& prof = *tbl_profile;

        std::string name = prof[PP_NAME].value_or(Profile::NOT);
        std::string url  = prof[PP_REPO_URL].value_or("");
        tern is_public   = prof[PP_REPO_PUB].value_or(tern::neutr);
        bool is_extern   = prof[PP_EXTERNAL].value_or(false);

        // `is_public.boolable()` checks for if `is_public` is not `neutr`
        if (name==Profile::NOT || url.empty() || !is_public.boolable()) {
            rep.addComplain("Profile have missing properties, skipping..");
            continue;
        }
        // is_public is already .boolable() if this code runs
        profiles.push_back(Profile{name, url, (bool)is_public, is_extern});
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
    Report rep;
    auto pair = m_toml->table.insert_or_assign(P_ACTIVE_PROF, name);
    if (pair.second == false) rep.addComplain("Active profile was already set.");
    return rep;
}


Report MCP::wSetDefaultEditor(const strview editor) {
    Report rep;
    auto pair = m_toml->table.insert_or_assign(P_CFG_EDITOR, editor);
    if (pair.second == false) rep.addComplain("Default editor was already set.");
    return rep.Good();
}

Report MCP::wAddProfile(const Profile& prof) {
    toml::table entry;
    entry.insert(PP_NAME, prof.name);
    entry.insert(PP_REPO_PUB, prof.is_pub);
    entry.insert(PP_EXTERNAL, prof.is_ext);
    entry.insert(PP_REPO_URL, prof.repo_url);

    // Inert profiles array if it doesn't exist yet
    if (!m_toml->table[P_PROFILES].as_array()) {
        m_toml->table.insert(P_PROFILES, toml::array{});
    }
    // push new profile entry to profiles array
    m_toml->table[P_PROFILES].as_array()->push_back(entry);

    return Report::Good();
}


Report MCP::wRemoveProfile(const strview name) {
    auto* arr_profiles = m_toml->table[P_PROFILES].as_array();
    if (arr_profiles == nullptr) {
        return Report::Bad("Profiles array is empty");
    }

    for (uint32 i=0;  i < arr_profiles->size();  ++i) {
        if (arr_profiles->at(i).at_path(PP_NAME) == name) {
            arr_profiles->erase(arr_profiles->begin() + i);
            return Report::Good();
        }
    }

    return Report::Bad("Profile '{}' doesn't exist!", name);
}


Report MCP::wSaveConfig(const fs::path& path) {
    std::ofstream fo(path, std::ios::out);
    if (!fo) return Report::Bad("Couldn't open output file: '{}'", path.string());
    fo << m_toml->table;
    return Report::Good();
}
