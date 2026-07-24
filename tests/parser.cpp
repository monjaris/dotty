#include "../core/include/DotlangParser.hpp"

static const std::string source[]
{
    "#! allow-sudo, allow-idk",
};

int main()
{
    Lexer lexer;
    ConfigParser cfp;
    Report report;

    for (auto i=std::begin(source);  i < std::end(source);  ++i) {
        // cm::debug("feeding lexer..");
        lexer.feed(*i);
        cm::debug("lexing line..");
        if (lexer.lexMain().error()) {
            cm::debug("BAD LEX!\n");
        }
        cm::debug("tokens:");
        lexer.print();
        // cm::debug("feeding parser..");
        cfp.feed(lexer.result());
        cm::debug("parsing tokens..");
        report = cfp.parseMain();
    }

    if (report.error()) {
        cm::debug("BAD!\n");
    } else {
        cm::debug("GOOD!\n");
    }
    report.printComplains();

}
