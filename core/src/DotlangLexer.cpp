#include "DotlangLexer.hpp"

using DL = DotlangLexer;


[[nodiscard]] char DL::m_seek() {
    return line[pos];
}

bool DL::m_checks() {
    return line.size() > pos;
}

void DL::m_step(uint32 n) {
    while(m_checks() && n--) { ++pos; }
}

void DL::m_skipws() {
    while(m_checks() && m_seek()==' ') m_step();
}



// returns `LexRes` between '#' and the last identifier in comma seperated list
// "# firstword, second_word, third-word" will return "firstword,second_word,third-word"
LexRes DL::lexDirectiveLine() {
    std::string directives;
    m_step(); // skip '#'
    m_step(); // skip '!'
    while (m_checks()) {
        char c = m_seek();
        if (c == '\0') break;
        if (::isspace(c)) { m_step(); continue; }
        if (c == ',') { directives += c; m_step(); continue; }
        if (::isalpha(c) || c=='-' || c=='_') { directives += c; m_step(); continue; }
        return LexRes::Bad();
    }
    return LexRes::Good(directives);
}


LexRes DL::lexAction() {
    std::string ident;
    m_step(); // skip '@'
    while (m_checks()) {
        char c = m_seek();
        if (::isalpha(c) || c == '-' || c == '_') {
            ident += c;
            m_step();
        }
        else break;
    }
    if (ident.empty()) return LexRes::Bad();
    return LexRes::Good(ident);
}


LexRes DL::lexString() {
    m_step();
    std::string str;
    while (m_checks()) {
        if (m_seek() == '"') break;
        if (str.size() > PATH_MAX) {
            // "String length is beyond platform's maximum {PATH_MAX}"
            return LexRes::Bad();
        }
        str += m_seek();
        m_step();
    }

    m_step();
    return LexRes::Good(str);
}


LexRes DL::lexCopier() {
    std::string copier = ">";
    m_step();
    if (m_seek() == '>') {
        copier += m_seek();
        m_step();
    } else return LexRes::Bad();
    return LexRes::Good(copier);
}


LexRes DL::lexLinker() {
    std::string linker = "-";
    m_step();
    if (m_seek() == '>') {
        linker += m_seek();
        m_step();
    } else return LexRes::Bad();
    return LexRes::Good(linker);
}


LexRes DL::lexDirCopier() {
    std::string dir_copier = ">";
    m_step();
    if (m_seek() == '>') {
        dir_copier += m_seek();
        m_step();
    } else return LexRes::Bad();
    if (m_seek() == '*') {
        dir_copier += m_seek();
        m_step();
    }
    else return LexRes::Bad();
    return LexRes::Good(dir_copier);
}


LexRes DL::lexDirLinker() {
    std::string dir_copier = "-";
    m_step();
    if (m_seek() == '>') {
        dir_copier += m_seek();
        m_step();
    } else return LexRes::Bad();
    if (m_seek() == '*') {
        dir_copier += m_seek();
        m_step();
    }
    else return LexRes::Bad();
    return LexRes::Good(dir_copier);
}


LexRes DL::lexIdent() {
    std::string ident;
    while (m_checks()) {
        if (::isalpha(m_seek()) || m_seek() == '.' || m_seek() == '-' || m_seek() == '_') {
            ident += m_seek();
            m_step();
        }
        else return LexRes::Good(ident);
    }
    return LexRes::Good(ident);
}


LexRes DL::lexEqual() {
    m_step();
    return LexRes::Good("=");
}


std::string DL::RemoveComment(std::string line) {
    std::vector<int32> quotes;
    quotes.reserve(128);

    for (uint32 i=0;  i < line.size();  ++i) {
        if (line[i] == '\"') quotes.push_back(i);

        if (core::is_even(quotes.size())) {
            if (line[i] == CMNT) {
                // handle for directive(#!)
                if (i == line.size()-1) {
                    return line.substr(0, i);
                } else if(line[i+1] != '!') {
                    return line.substr(0, i);
                }
            }
        }
    }

    return line;
}



void DL::feed(const strview input) {
    line = input;
}

void DL::feed(std::string&& input) {
    line = std::move(input);
}


void DL::print() {
    for (uint32 i=0;  i < tokens.size();  ++i) {
        core::print<false>((char)tokens[i].type, " : '", tokens[i].name, "'\n");
    }
    core::print<true>('\n');
}



Report DL::lexMain() {
    tokens.clear();
    Report report;
    pos = 0;
    Token maintok = {Token::NONE, "<none>"};

    while(m_checks())
    {
        line = RemoveComment(line);
        m_skipws();

        // Lex Directive
        if (m_seek() == '#') {
            auto lex = lexDirectiveLine();
            if (lex.success()) {
                maintok.name = lex.val();
                maintok.type = Token::DIRECTIVE;
            } else {
                report.addComplain("Couldn't lex directive"); continue;
            }
        }
        // Lex ACTION
        else if (m_seek() == '@') {
            auto lex = lexAction();
            if (lex.success()) {
                maintok.name = lex.val();
                maintok.type = Token::ACTION;
            } else {
                report.addComplain("Couldn't lex action"); continue;
            }
        }
        // Lex STRING
        else if (m_seek() == '"') {
            auto lex = lexString();
            if (lex.success()) {
                maintok.name = lex.val();
                maintok.type = Token::STRING;
            } else {
                report.addComplain("Couldn't lex string"); continue;
            }
        }
        // Lex COPIER && DIR_COPIER
        else if (m_seek() == '>') {
            uint32 save_pos = pos;
            auto lex1 = lexDirCopier();

            if (lex1.success()) {  // on fail, check for Token::LEX_COPIER
                maintok.name = lex1.val();
                maintok.type = Token::DIR_COPIER;
            } else {
                pos = save_pos;
                auto lex2 = lexCopier();

                if (lex2.success()) {
                    maintok.name = lex2.val();
                    maintok.type = Token::COPIER;
                } else {
                    report.addComplain("Couldn't lex copy operator");
                    pos = save_pos; continue;
                }
            }
        }
        // Lex LINKER && DIR_LINKER
        else if (m_seek() == '-') {
            // same flow with lexing in previous branch
            uint32 save_pos = pos;
            auto lex1 = lexDirLinker();

            if (lex1.success()) {
                maintok.name = lex1.val();
                maintok.type = Token::DIR_LINKER;
            } else {
                pos = save_pos;
                auto lex2 = lexLinker();

                if (lex2.success()) {
                    maintok.name = lex2.val();
                    maintok.type = Token::LINKER;
                } else {
                    report.addComplain("Couldn't lex link operator");
                    pos = save_pos; continue;
                }
            }
        }
        else if (isalpha(m_seek())) {
            auto lex = lexIdent();
            if (lex.success()) {
                maintok.name = lex.val();
                maintok.type = Token::IDENT;
            } else {
                report.addComplain("Couldn't lex identifier"); continue;
            }
        }
        else if (m_seek() == '=') {
            auto lex = lexEqual();
            if (lex.success()) {
                maintok.name = lex.val();
                maintok.type = Token::EQUAL;
            } else {
                report.addComplain("Couldn't lex equal operator"); continue;
            }
        }
        else
        {
            if (m_seek() < ' ') continue;  // dont error, ignore if it's '\0' or any char below ' '
            core::debug("Lexer::lexMain(): Encountered unknown character: '", m_seek(), "'");
            if (m_seek() == '\0') core::debug("and its null\n");
            tokens.emplace_back(maintok.type=Token::UNKNOWN, maintok.name="<error>");
            m_step();
            continue;
        }

        m_skipws();
        tokens.emplace_back(maintok);
    }

    return report;
}


auto DL::result() -> decltype(tokens) & {
    return tokens;
}
