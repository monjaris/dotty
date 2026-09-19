#pragma once
#include "DotlangLexer.hpp"

/*
 * Parser for dotlang — DSL for dotty config mappings and actions.
 *
 * Language specification:
 *   comment lines start with '#' (except "#!" directives)
 *   multi-line comments are not supported
 *
 * Directives (file-scoped enable switches):
 *   #!allow-sudo
 *   #!allow-exec
 *   #!allow-sudo,allow-exec
 *
 * Actions (require matching directive to be enabled):
 *   @sudo  "<src>" >>  "<dest>"     # privileged copy file
 *   @sudo  "<src>" ->  "<dest>"     # privileged link file
 *   @sudo  "<src>" >>* "<dest>"     # privileged copy dir
 *   @sudo  "<src>" ->* "<dest>"     # privileged link dir
 *   @exec  'command args...'        # posix_spawn command (single quotes)
 *
 * Plain mappings (no action prefix):
 *   "<src>" >>  "<dest>"
 *   "<src>" ->  "<dest>"
 *   "<src>" >>* "<dest>"
 *   "<src>" ->* "<dest>"
 *
 * Notes:
 *   Both '-' and '_' count as identifier characters.
 *   Identifiers must start with an alpha character.
 */


NAMESPACE_START()

class Action {
    bool m_enabled = false;

public:
    const char* directive;
    const char* command;

    explicit Action (const char* directive, const char* command)
    : directive(directive), command(command) {}

    bool is_enabled() const {
        return m_enabled;
    }

    void enable() {
        m_enabled = true;
    }

    void reset() {
        m_enabled = false;
    }

    bool operator== (const Action& other) const {
        return !strcmp(command, other.command);
    }
};

NAMESPACE_END()


// To fix the fact that if branches of token type checks can fail
struct ParseReport : Report {
    bool matched = false;
};


class DotlangParser
{
private:
    uint32 idx = { 0uz };

    struct Opts {
        Action sudo {"allow-sudo", "sudo"};
        Action exec {"allow-exec", "exec"};
    } opts;

public:
    std::vector<Token> tokens;
    // regular path ops
    std::vector<SrcDest> copy_files;
    std::vector<SrcDest> copy_dirs;
    std::vector<SrcDest> link_files;
    std::vector<SrcDest> link_dirs;
    // privileged path ops
    std::vector<SrcDest> sudo_copy_files;
    std::vector<SrcDest> sudo_copy_dirs;
    std::vector<SrcDest> sudo_link_files;
    std::vector<SrcDest> sudo_link_dirs;
    // @exec command lines (single-quoted payload, no shell)
    std::vector<std::string> exec_commands;

private:
    Token m_get();
    bool m_checks();
    void m_advance();

    ParseReport m_parseAction();
    ParseReport m_parsePathOperation(inilist<Action> options);
    ParseReport m_parseDirectives();
    ParseReport m_parseExecAction();

public:
    static void ResolvePaths(std::string* src, std::string* dest);

    void feed(std::vector<Token>&& tokens);
    void feed(const std::vector<Token>& tokens);

    // Reset enable flags and clear result vectors (call once per config file).
    void resetFileState();

    ParseReport parseMain();
};
