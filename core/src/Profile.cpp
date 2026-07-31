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
