#pragma once
#include "Profile.hpp"

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
