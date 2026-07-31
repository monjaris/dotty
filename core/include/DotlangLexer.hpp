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
        ACTION='@',
        COLON=':',
        IDENT='i',
        EQUAL='=',
        // sentinel
        NONE='_',
        UNKNOWN='!'
    } type;
    std::string name;
};


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


class DotlangLexer {
private:
    std::string line;
    uint32 pos;
    std::vector<Token> tokens;
    static constexpr char CMNT = '#';

private:
    [[nodiscard]] char get();
    bool checks();
    void step(uint32 n=1);
    void skipws();

    LexRes lexString();
    LexRes lexCopier();
    LexRes lexLinker();
    LexRes lexDirCopier();
    LexRes lexDirLinker();
    LexRes lexDirectiveLine();
    LexRes lexIdent();
    LexRes lexEqual();

public:

    [[nodiscard]] static std::string RemoveComment(std::string line);

    void feed(const strview input);
    void feed(std::string&& input);
    void print();
    Report lexMain();
    decltype(tokens)& result();
};
