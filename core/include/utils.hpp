#pragma once
NAMESPACE_START(core)

[[nodiscard]] inline std::string strip_nl(std::string input) {
    std::string str = std::move(input);
    str.erase(std::remove(str.begin(), str.end(), OS_NEWLN), str.end());
    return str;
}


// returns subtring until first space
inline std::string get_first_word(strview strv) {
    for (usize i=0;  i < strv.size();  ++i) {
        if (::isspace(strv[i])) {
           return strv.substr(0, i).data();
        }
    }
    return strv.data();
}


// simply, adds slash between two strings and returns
inline fs::path cat_path(const fs::path& parent, const fs::path& child) {
    return parent / child;
}


NAMESPACE_END(core)
