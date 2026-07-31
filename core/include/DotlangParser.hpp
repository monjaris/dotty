#pragma once
#include "DotlangLexer.hpp"

/*
 * This file contains Parser for dotlang
 * dotlang is DSL for dotty project to optimize scripting for dotty's purposes
 *
 * Language specification:
 * comment lines start with '#' character
 * multi-line comment doesn't exist yet and i wont implement it as long as users doesnt want it
 *
 * Small notes put for clarity:
 * Both '-' and '_' are counted as identifier elements, therefore, `allow-sudo` is an identifier.
 * But _dummy1 or -dummy2 are not identifiers, they start with non-alpha character
 * Also, foo.bar.smt is an identifier too, but it's flat and just visualizes member access
 */


// anonymus namespace, private to other files
NAMESPACE_START()

class Action {
    bool m_enabled = false;

public:
    const char* directive;

    Action (const char* directive): directive(directive) {}

    void enable(bool enabled = true) {
        m_enabled = enabled;
    }

    // for comparison trait
    bool operator== (const Action& other) const {
        return directive == other.directive;
    }
};

NAMESPACE_END()


struct ParseReport : Report {
    bool matched = false;
};


class DotlangParser
{
private:
    uint32 idx = { 0uz };

    struct Options {
        Action sudo {"allow-sudo"};
    } opts;

public:
    std::vector<Token> tokens;
    // these four are for storing copy/link action paths
    std::vector<SrcDest> copy_files;
    std::vector<SrcDest> copy_dirs;
    std::vector<SrcDest> link_files;
    std::vector<SrcDest> link_dirs;
    // these four are previleged versions of above four
    std::vector<SrcDest> sudo_copy_files;
    std::vector<SrcDest> sudo_copy_dirs;
    std::vector<SrcDest> sudo_link_files;
    std::vector<SrcDest> sudo_link_dirs;

private:
    Token m_get();
    bool m_checks();
    void m_advance();


    ParseReport m_parseAction();
    ParseReport m_parsePathOperation(inilist<Action> options);
    ParseReport m_parseDirectives();

public:
    static void ResolvePaths(std::string* src, std::string* dest);

    void feed(std::vector<Token>&& tokens);
    void feed(const std::vector<Token>& tokens);

    ParseReport parseMain();
};
