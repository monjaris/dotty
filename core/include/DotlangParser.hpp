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


