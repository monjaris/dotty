#include "DotlangParser.hpp"


using DP = DotlangParser;


Token DP::m_get() {
    if (!m_checks()) return Token{Token::NONE, "<none>"};
    return tokens[idx];
}

bool DP::m_checks() {
    return tokens.size() > idx;
}

void DP::m_advance() {
    if (m_checks()) ++idx;
}


void DP::ResolvePaths(std::string* src, std::string* dest) {
    *src = core::parsePathTilde(*src);
    if (dest->ends_with("/..")) {
        dest->replace(dest->size() - 2, 2, fs::path(*src).filename());
    } else if (*dest == "..") {
        *dest = fs::path(*src).filename();
    }
}


void DP::feed(std::vector<Token>&& tokens) {
    this->tokens = std::move(tokens);
    this->idx = 0;
}

void DP::feed(const std::vector<Token>& tokens) {
    this->tokens = tokens;
    this->idx = 0;
}


void DP::resetFileState() {
    opts.sudo.reset();
    opts.exec.reset();
    copy_files.clear();
    copy_dirs.clear();
    link_files.clear();
    link_dirs.clear();
    sudo_copy_files.clear();
    sudo_copy_dirs.clear();
    sudo_link_files.clear();
    sudo_link_dirs.clear();
    exec_commands.clear();
}


ParseReport DP::m_parsePathOperation(inilist<Action> options)
{
    ParseReport report;

    if (Token left = m_get(); left.type == Token::STRING && (report.matched = true)) {
        m_advance();

        // 1.  >>
        if (Token oper = m_get(); oper.type == Token::COPIER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get(); right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_copy_files.emplace_back(SrcDest{left.name, right.name});
                } else {
                    copy_files.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            } else {
                report.addComplain("Expected STRING after COPIER operator");
                report.matched = false;
            }
        }
        // 2.  ->
        else if (Token oper = m_get(); oper.type == Token::LINKER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get(); right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_link_files.emplace_back(SrcDest{left.name, right.name});
                } else {
                    link_files.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            } else {
                report.addComplain("Expected STRING after LINKER operator");
                report.matched = false;
            }
        }
        // 3.  >>*
        else if (Token oper = m_get(); oper.type == Token::DIR_COPIER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get(); right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_copy_dirs.emplace_back(SrcDest{left.name, right.name});
                } else {
                    copy_dirs.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            } else {
                report.addComplain("Expected STRING after DIR-COPIER operator");
                report.matched = false;
            }
        }
        // 4.  ->*
        else if (Token oper = m_get(); oper.type == Token::DIR_LINKER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get(); right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_link_dirs.emplace_back(SrcDest{left.name, right.name});
                } else {
                    link_dirs.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            } else {
                report.addComplain("Expected STRING after DIR-LINKER operator");
                report.matched = false;
            }
        }
        else {
            report.matched = false;
            report.addComplain("Expected operator after STRING");
        }
    }

    return report;
}


ParseReport DP::m_parseExecAction()
{
    ParseReport report;

    // Expect a STRING token holding the command line (from single quotes)
    if (Token cmd = m_get(); cmd.type == Token::STRING && (report.matched = true)) {
        m_advance();
        if (cmd.name.empty()) {
            report.addComplain("@exec requires a non-empty command string");
            report.matched = false;
            return report;
        }
        exec_commands.push_back(cmd.name);
    } else {
        report.addComplain("Expected single-quoted STRING after @exec");
        report.matched = false;
    }

    return report;
}


ParseReport DP::m_parseAction()
{
    ParseReport report;

    if (m_checks() && m_get().type == Token::ACTION && (report.matched = true)) {
        Token lex = m_get();
        m_advance();

        if (lex.name == opts.sudo.command) {
            if (!opts.sudo.is_enabled()) {
                report.addComplain(
                    "'{}' used without '#!{}' directive", opts.sudo.command, opts.sudo.directive
                );
                report.matched = false;
                return report;
            }
            report = m_parsePathOperation({opts.sudo});
        }
        else if (lex.name == opts.exec.command) {
            if (!opts.exec.is_enabled()) {
                report.addComplain(
                    "'{}' used without '#!{}' directive", opts.exec.command, opts.exec.directive
                );
                report.matched = false;
                return report;
            }
            report = m_parseExecAction();
        }
        else {
            report.addComplain("Unknown action: '{}'", lex.name);
            report.matched = false;
        }
    }

    return report;
}


ParseReport DP::m_parseDirectives()
{
    ParseReport report;

    if (m_checks() && m_get().type == Token::DIRECTIVE && (report.matched = true)) {
        std::string directive_list = m_get().name;

        for (auto&& d : directive_list | std::views::split(',')) {
            std::string name(d.begin(), d.end());
            // trim incidental whitespace that may survive lexing
            while (!name.empty() && ::isspace(static_cast<unsigned char>(name.front())))
                name.erase(name.begin());
            while (!name.empty() && ::isspace(static_cast<unsigned char>(name.back())))
                name.pop_back();

            if (name.empty()) continue;

            if (name == opts.sudo.directive) {
                opts.sudo.enable();
            }
            else if (name == opts.exec.directive) {
                opts.exec.enable();
            }
            else {
                report.addComplain("Unknown directive: '{}'", name);
            }
        }

        m_advance();
    }
    else {
        report.matched = false;
    }

    return report;
}


ParseReport DP::parseMain()
{
    ParseReport report;

    // NOTE: enable flags are NOT reset here. Call resetFileState() once
    // before parsing a whole config file. This lets directives on earlier
    // lines enable actions on later lines when tokens are concatenated,
    // and also lets per-line callers keep state across lines if they want.

    // Optional leading directive(s) - may appear anywhere in the token stream
    // as long as they come before the action that needs them.
    while (m_checks() && m_get().type == Token::DIRECTIVE) {
        auto dr = m_parseDirectives();
        if (dr.error()) report.addComplain("{}", dr.m_msg);
    }

    while (m_checks()) {
        // Bare IDENT "sudo" prefix (legacy): sudo "/path" >> "dest"
        if (m_get().type == Token::IDENT && m_get().name == opts.sudo.command) {
            if (!opts.sudo.is_enabled()) {
                report.addComplain(
                    "'{}' used without '#!{}' directive", opts.sudo.command, opts.sudo.directive
                );
                m_advance();
                continue;
            }
            m_advance();
            if (auto pr = m_parsePathOperation({opts.sudo}); pr.matched) {
                if (pr.error()) report.addComplain("{}", pr.m_msg);
                continue;
            }
        }

        // @action ...
        if (m_get().type == Token::ACTION) {
            if (auto ar = m_parseAction(); ar.matched) {
                if (ar.error()) report.addComplain("{}", ar.m_msg);
                continue;
            }
            // matched=false after ACTION means error already recorded
            if (!m_checks()) break;
            // skip the rest of a bad action line
            m_advance();
            continue;
        }

        // Plain path operation
        if (auto pr = m_parsePathOperation({}); pr.matched) {
            if (pr.error()) report.addComplain("{}", pr.m_msg);
            continue;
        }

        // Another directive mid-stream
        if (m_get().type == Token::DIRECTIVE) {
            auto dr = m_parseDirectives();
            if (dr.error()) report.addComplain("{}", dr.m_msg);
            continue;
        }

        report.addComplain("Unexpected token: '{}'", m_get().name);
        m_advance();
    }

    return report;
}
