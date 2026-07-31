#include "DotlangParser.hpp"


using DP = DotlangParser;


Token DP::m_get() {
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
    *src = cm::parsePathTilde(*src);
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

    if (Token left = m_get();  left.type == Token::STRING) {
        m_advance();

        // 1.  >>
        if (Token oper = m_get();  oper.type == Token::COPIER) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING) {
                ResolvePaths(&left.name, &right.name);
                if (cm::contains(options, opts.sudo)) {
                    sudo_copy_files.emplace_back(SrcDest{left.name, right.name});
                } else {
                    copy_files.emplace_back(SrcDest{left.name, right.name});
                }
            }
            else report.addComplain("Expected STRING after COPIER operator\n");
        }
        // 2.  ->
        if (Token oper = m_get();  oper.type == Token::LINKER) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING) {
                ResolvePaths(&left.name, &right.name);
                link_files.emplace_back(SrcDest{left.name, right.name});
            }
            else report.addComplain("Expected STRING after LINKER operator\n");
        }
        // 3.  >>*
        if (Token oper = m_get();  oper.type == Token::DIR_COPIER) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING) {
                ResolvePaths(&left.name, &right.name);
                copy_dirs.emplace_back(SrcDest{left.name, right.name});
            }
            else report.addComplain("Expected STRING after DIR-COPIER operator\n");
        }
        // 4.  ->*
        if (Token oper = m_get();  oper.type == Token::DIR_LINKER) {
            m_advance();

            if (Token right = m_get();  right.type == Token::STRING) {
                ResolvePaths(&left.name, &right.name);
                link_dirs.emplace_back(SrcDest{left.name, right.name});
            }
            else report.addComplain("Expected STRING after DIR-LINKER operator\n");
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

    if (m_checks() && m_get().type == Token::ACTION) {
        Token lex = m_get();
        Action opt = Action(lex.name.c_str());

        report = m_parsePathOperation({opt});
    }

    return report;
}


ParseReport DP::m_parseDirectives()
{
    ParseReport report;

    if (m_checks() && m_get().type == Token::DIRECTIVE) {
        std::string directive_list = m_get().name;

        for (auto&& d  : directive_list | std::views::split(',')) {
            std::string name(d.begin(), d.end());
            if (name == "allow-sudo") {
                opts.sudo.enable(true);
            }
            else
            {
                report.addComplain("Unknown directive: '{}'", name);
            }
        }

        m_advance();
    }

    return report;
}


ParseReport DP::parseMain()
{
    ParseReport report;

    copy_files.reserve(64);
    copy_dirs.reserve(64);
    link_files.reserve(64);
    link_dirs.reserve(64);
    sudo_copy_files.reserve(32);
    sudo_copy_dirs.reserve(32);
    sudo_link_files.reserve(32);
    sudo_link_dirs.reserve(32);


    // # -> SPACES -> DIRECTIVE -> COMMA? -> (DIRECTIVE...)
    report = m_parseDirectives();

    while(m_checks())
    {
        // call parsing functions
        //
        // if branch condition fails: return with `report.matched = false`
        // which results in m_advance() and continuing to next iteration
        //
        // if branch condition doesn't fail: return `report.matched = true`
        // which falls to erroring about unexpected token.

        // STRING -> OPER -> STRING
        if (report = m_parsePathOperation(inilist<Action>{});  !report.matched) {
            m_advance(); continue;
        }

        report.addComplain(
            "Unexpected token: '{}' with the type of <{}>",
            m_get().name, (char)m_get().type
        );

        m_advance();
    }

    return report;
}
