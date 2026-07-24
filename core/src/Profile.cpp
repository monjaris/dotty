#include "Profile.hpp"

Profile::Profile(
    const std::string& name,
    const std::string& repo_url,
    bool is_public, bool is_external
)
{
    this->name = name;
    this->repo_url = repo_url;
    this->is_pub = is_public;
    this->is_ext = is_external;
}

// std::string Profile::repoUrl() const {
    // return cm::make_repo_url(github_host, repo_name);
// }

// fs::path Profile::getConfigPath() const {
    // return  dotty.config_d/name/dotty.config_source;
// }

