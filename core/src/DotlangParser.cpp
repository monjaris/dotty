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


// Interdiamate parse function
void DP::ResolvePaths(std::string* src, std::string* dest) {
    // SRC resolution
    *src = core::parsePathTilde(*src);
    // DEST resolution
    if (dest->ends_with("/..")) { dest->replace(dest->size()-2, 2, fs::path(*src).filename()); }
    else if (*dest == "..") { *dest = fs::path(*src).filename(); }
}


void DP::feed(std::vector<Token>&& tokens) {
    this->tokens = std::move(tokens);
    this->idx = 0;
}

void DP::feed(const std::vector<Token>& tokens) {
    this->tokens = tokens;
    this->idx = 0;
}



ParseReport DP::m_parsePathOperation(inilist<Action> options)
{
    ParseReport report;

    if (Token left = m_get();  left.type == Token::STRING && (report.matched = true)) {
        m_advance();

        // 1.  >>
        if (Token oper = m_get();  oper.type == Token::COPIER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_copy_files.emplace_back(SrcDest{left.name, right.name});
                } else {
                    copy_files.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            }
            else
            {
                report.addComplain("Expected STRING after COPIER operator\n");
                report.matched = false;
            }
        }
        // 2.  ->
        else if (Token oper = m_get();  oper.type == Token::LINKER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_link_files.emplace_back(SrcDest{left.name, right.name});
                } else {
                    link_files.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            }
            else
            {
                report.addComplain("Expected STRING after LINKER operator\n");
                report.matched = false;
            }
        }
        // 3.  >>*
        else if (Token oper = m_get();  oper.type == Token::DIR_COPIER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_copy_dirs.emplace_back(SrcDest{left.name, right.name});
                } else {
                    copy_dirs.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            }
            else
            {
                report.addComplain("Expected STRING after DIR-COPIER operator\n");
                report.matched = false;
            }
        }
        // 4.  ->*
        else if (Token oper = m_get();  oper.type == Token::DIR_LINKER && (report.matched = true)) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING && (report.matched = true)) {
                ResolvePaths(&left.name, &right.name);
                if (core::contains(options, opts.sudo)) {
                    sudo_link_dirs.emplace_back(SrcDest{left.name, right.name});
                } else {
                    link_dirs.emplace_back(SrcDest{left.name, right.name});
                }
                m_advance();
            }
            else
            {
                report.addComplain("Expected STRING after DIR-LINKER operator\n");
                report.matched = false;
            }
        }

        else
        {
            report.matched = false;
            report.addComplain("Expected operator after STRING\n");
        }
    }

    return report;
}


ParseReport DP::m_parseAction()
{
    ParseReport report;

    if (m_checks() && m_get().type == Token::ACTION && (report.matched = true)) {
        Token lex = m_get();
        m_advance();

        if (lex.name == opts.sudo.command && opts.sudo.is_enabled()) {
            report = m_parsePathOperation({opts.sudo});
        }
        else if (lex.name == opts.sudo.command) {
            report.addComplain(
                "'{}' used without '#!{}' directive", opts.sudo.command, opts.sudo.directive
            );
            report.matched = false;
        }
        else
        {
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

        for (auto&& d  : directive_list | std::views::split(',')) {
            std::string name(d.begin(), d.end());
            if (name == opts.sudo.directive) {
                opts.sudo.enable();
            }

            else report.addComplain("Unknown directive: '{}'", name);
        }

        m_advance();
    }
    else
    {
        report.addComplain("Expected DIRECTIVE\n");
        report.matched = false;
    }

    return report;
}


ParseReport DP::parseMain()
{
    ParseReport report;
    // reset actions
    opts.sudo.reset();
    // preserve memory for path vectors
    copy_files.reserve(64);
    copy_dirs.reserve(64);
    link_files.reserve(64);
    link_dirs.reserve(64);
    sudo_copy_files.reserve(32);
    sudo_copy_dirs.reserve(32);
    sudo_link_files.reserve(32);
    sudo_link_dirs.reserve(32);


    // call parsing functions
    //
    // if branch condition fails: return with `report.matched = false`
    // which results in m_advance() and continuing to next iteration
    //
    // if branch condition doesn't fail: return `report.matched = true`
    // which falls to erroring about unexpected token.

    // # -> SPACES -> DIRECTIVE -> COMMA? -> (DIRECTIVE...)
    report = m_parseDirectives();

    while (m_checks()) {
        bool line_sudo = false;

        if (m_get().type == Token::IDENT && m_get().name == opts.sudo.command) {
            if (!opts.sudo.is_enabled()) {
                report.addComplain(
                    "'{}' used without '#!{}' directive", opts.sudo.command, opts.sudo.directive
                );
                m_advance();
                continue;
            }
            line_sudo = true;
            m_advance();
        }

        if (report = m_parsePathOperation(
                line_sudo ? inilist<Action>{opts.sudo} : inilist<Action>{}
            ); report.matched) {
            continue;
        }
        else if (report = m_parseAction(); report.matched) {
            continue;
        }

        report.addComplain("Unexpected token: '{}'", m_get().name);
        m_advance();
    }

    return report;
}
