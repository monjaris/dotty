#pragma once
#include "profile.hpp"

/*
 * This file contains Tokenizer and Parser for dotty-script
 * dotty-script is the DSL of dotty project to optimize this specific development area
 *
 * Language specification:
 * comment lines start with '#' character
 * multi-line comment doesn't exist yet and i wont implement it as long as users doesnt want it
 *
 * Small notes put for clarity:
 * I prefer identifiers with '-' instead of '_', therefore, `allow-sudo` is an identifier.
 * Also, foo.bar.smt is an identifier too, but it's flat and just visualizes member access
 */


struct Token {
    enum TT {
        // values
        STRING='s',
        // operators
        COPIER='c',
        LINKER='l',
        DIR_COPIER='C',
        DIR_LINKER='L',
        // punctuators
        DIRECTIVE='!',
        COLON=':',
        IDENT='i',
        EQUAL='=',
        // sentinel
        NONE='_',
        UNKNOWN='!'
    } type;
    std::string name;
};

class Lexer {
private:
    std::string line;
    uint32 pos;
    std::vector<Token> tokens;
    static constexpr char CMNT = '#';

    class [[nodiscard]] LexRes {
        std::string str = "<untouched>";
        bool bad = false;
        LexRes (const std::string value, bool bad)
            : str(value), bad(bad) {}
    public:
        static COMPTIME_STR ERROR = "<lex-error>";
        static LexRes Good(const std::string& value) { return {value, false}; }
        static LexRes Bad() { return LexRes(ERROR, true); }
        const std::string& val() const { return str; }
        bool err() const { return bad; }
        bool success() const { return !bad; }
    };

private:
    [[nodiscard]] char get() { return line[pos]; }
    bool checks() { return line.size() > pos; }
    void step(uint32 n=1) { while(checks() && n--) { ++pos; } }
    void skipws() { while(checks() && get()==' ') step(); }

    LexRes lexString() {
        step();
        std::string str;
        while (checks()) {
            if (get() == '"') break;
            if (str.size() > PATH_MAX) {
                // "String length is beyond platform's maximum {PATH_MAX}"
                return LexRes::Bad();
            }
            str += get();
            step();
        }

        step();
        return LexRes::Good(str);
    }


    LexRes lexCopier() {
        std::string copier = ">";
        step();
        if (get() == '>') {
            copier += get();
            step();
        } else return LexRes::Bad();
        return LexRes::Good(copier);
    }

    LexRes lexLinker() {
        std::string linker = "-";
        step();
        if (get() == '>') {
            linker += get();
            step();
        } else return LexRes::Bad();
        return LexRes::Good(linker);
    }

    LexRes lexDirCopier() {
        std::string dir_copier = ">";
        step();
        if (get() == '>') {
            dir_copier += get();
            step();
        } else return LexRes::Bad();
        if (get() == '*') {
            dir_copier += get();
            step();
        }
        else return LexRes::Bad();
        return LexRes::Good(dir_copier);
    }

    LexRes lexDirLinker() {
        std::string dir_copier = "-";
        step();
        if (get() == '>') {
            dir_copier += get();
            step();
        } else return LexRes::Bad();
        if (get() == '*') {
            dir_copier += get();
            step();
        }
        else return LexRes::Bad();
        return LexRes::Good(dir_copier);
    }


    // returns `LexRes` between '#' and the last identifier in comma seperated list
    // "# firstword, second_word, third-word" will return "firstword,second_word,third-word"
    LexRes lexDirectiveLine() {
        std::string directives;
        step(); // skip '#'
        step(); // skip '!'
        while (checks()) {
            char c = get();
            if (c == '\0') break;
            if (::isspace(c)) { step(); continue; }
            if (c == ',') { directives += c; step(); continue; }
            if (::isalpha(c) || c=='-' || c=='_') { directives += c; step(); continue; }
            return LexRes::Bad();
        }
        return LexRes::Good(directives);
    }


    LexRes lexIdent() {
        std::string ident;
        while (checks()) {
            if (::isalpha(get()) || get() == '.' || get() == '-' || get() == '_') {
                ident += get();
                step();
            }
            else return LexRes::Bad();
        }
        return LexRes::Good(ident);
    }

    LexRes lexEqual() {
        step();
        return LexRes::Good("=");
    }


public:

    [[nodiscard]]
    static std::string RemoveComment(std::string line) {
        std::vector<int32> quotes;
        quotes.reserve(128);

        for (uint32 i=0;  i < line.size();  ++i) {
            if (line[i] == '\'') {
                quotes.push_back(i);
            }

            if (cm::is_even(quotes.size())) {
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

    void feed(const strview input) {
        line = input;
    }

    void feed(std::string&& input) {
        line = std::move(input);
    }


    void print() {
        for (uint32 i=0;  i < tokens.size();  ++i) {
            cm::print<false>((char)tokens[i].type, " : '", tokens[i].name, "'\n");
        }
        cm::print<true>('\n');
    }


    Report lexMain() {
        tokens.clear();
        Report report;
        pos = 0;
        Token maintok = {Token::NONE, "<none>"};

        while(checks())
        {
            line = RemoveComment(line);
            skipws();

            if (get() == '#') {
                auto lex = lexDirectiveLine();
                if (lex.success()) {
                    maintok.name = lex.val();
                    maintok.type = Token::DIRECTIVE;
                } else {
                    report.addComplain("Couldn't lex directive"); continue;
                }
            }
            else if (get() == '"') {
                auto lex = lexString();
                if (lex.success()) {
                    maintok.name = lex.val();
                    maintok.type = Token::STRING;
                } else {
                    report.addComplain("Couldn't lex string"); continue;
                }
            }
            // Lex COPIER && DIR_COPIER
            else if (get() == '>') {
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
            else if (get() == '-') {
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
            else if (isalpha(get())) {
                auto lex = lexIdent();
                if (lex.success()) {
                    maintok.name = lex.val();
                    maintok.type = Token::IDENT;
                } else {
                    report.addComplain("Couldn't lex identifier"); continue;
                }
            }
            else if (get() == '=') {
                auto lex = lexEqual();
                if (lex.success()) {
                    maintok.name = lexEqual().val();
                    maintok.type = Token::EQUAL;
                } else {
                    report.addComplain("Couldn't lex equal operator"); continue;
                }
            }
            else
            {
                if (get() < ' ') continue;  // dont error, ignore if it's '\0' or any char below ' '
                cm::debug("Lexer::lexMain(): Encountered unknown character: '", get(), "'");
                if (get() == '\0') cm::debug("and its null\n");
                tokens.emplace_back(maintok.type=Token::UNKNOWN, maintok.name="<error>");
                step();
                continue;
            }

            skipws();
            tokens.emplace_back(maintok);
        }

        return report;
    }

    auto& result() {
        return tokens;
    }
};


class ConfigParser {
private:
    uint32 idx = { 0uz };
public:
    std::vector<Token> tokens;
    // these four are for storing copy/link action paths
    std::vector<SrcDest> copy_files;
    std::vector<SrcDest> copy_dirs;
    std::vector<SrcDest> link_files;
    std::vector<SrcDest> link_dirs;
    // std::vector<std::string> directives;

private:
    struct {
        bool sudo = false;
    } opt_enabled;

    Token get() { return tokens[idx]; }
    bool checks() { return tokens.size() > idx; }
    void advance() { if (checks()) ++idx; }

    // Interdiamate parse function
    void parsePaths(std::string* src, std::string* dest) {
        // SRC parsing
        *src = cm::parsePathTilde(*src);
        // DEST parsing
        if (dest->ends_with("/..")) { dest->replace(dest->size()-2, 2, fs::path(*src).filename()); }
        else if (*dest == "..") { *dest = fs::path(*src).filename(); }
    }


public:

    void feed(std::vector<Token>&& tokens) {
        this->tokens = std::move(tokens);
        this->idx = 0;
    }

    void feed(const std::vector<Token>& tokens) {
        this->tokens = tokens;
        this->idx = 0;
    }


    Report parseMain()
    {
        copy_files.reserve(64);
        copy_dirs.reserve(64);
        link_files.reserve(64);
        link_dirs.reserve(64);
        //
        // directives.reserve(4);

        Report rep;

        // parse directive
        if (checks() && get().type == Token::DIRECTIVE) {
            std::string directive_list = get().name;

            for (auto&& d  : directive_list | std::views::split(',')) {
                std::string name(d.begin(), d.end());
                if (name == "allow-sudo") {
                    opt_enabled.sudo = true;
                }
                else
                {
                    rep.addComplain("Unknown directive: '{}'", name);
                }
            }

            advance();
        }


        while(checks())
        {
            // STRING -> OP -> STRING
            // # -> SPACES -> DIRECTIVE -> COMMA? -> (DIRECTIVE...)

            if (get().type == Token::STRING) {
                std::string src = get().name;
                advance();

                if (get().type == Token::COPIER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        copy_files.emplace_back(SrcDest{src, dest});
                    }
                    else rep.addComplain("Expected STRING after COPIER operator\n");
                }
                else if (get().type == Token::LINKER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        link_files.emplace_back(SrcDest{src, dest});
                    }
                    else rep.addComplain("Expected STRING after LINKER operator\n");
                }
                else if (get().type == Token::DIR_COPIER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        copy_dirs.emplace_back(SrcDest{src, dest});
                    }
                    else rep.addComplain("Expected STRING after DIR-COPIER operator\n");
                }
                else if (get().type == Token::DIR_LINKER) {
                    ;
                    advance();
                    if (get().type == Token::STRING) {
                        std::string dest = get().name;
                        parsePaths(&src, &dest);
                        link_dirs.emplace_back(SrcDest{src, dest});
                    }
                    else rep.addComplain("Expected STRING after DIR-LINKER operator\n");
                }
                else rep.addComplain("Expected operator after STRING\n");
            }  // if STRING

            else
            {
                rep.addComplain(
                    "Unexpected token: '{}' with the type of <{}>",
                    get().name, (char)get().type
                );
            }

            advance();
        }

        return rep;
    }
};


