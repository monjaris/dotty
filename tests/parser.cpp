#include "DotlangParser.hpp"
#include <cassert>
#include <iostream>

static uint32 failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++failures; \
        } else { \
            std::cerr << "[ OK ] " << msg << "\n"; \
        } \
    } while (0)


static std::vector<Token> lex_line(const std::string& line) {
    DotlangLexer lexer;
    lexer.feed(line);
    Report r = lexer.lexMain();
    r.printComplains();
    return lexer.result();
}


void test_basic_copy() {
    auto tokens = lex_line("\"/etc/hosts\" >> \"hosts\"");
    DotlangParser p;
    p.feed(tokens);
    auto report = p.parseMain();

    CHECK(p.copy_files.size() == 1, "basic copy: one entry in copy_files");
    CHECK(p.sudo_copy_files.empty(), "basic copy: sudo_copy_files stays empty");
    if (!p.copy_files.empty()) {
        CHECK(p.copy_files[0].dest == "hosts", "basic copy: dest resolved correctly");
    }
}


void test_basic_link() {
    auto tokens = lex_line("\"/etc/nvim\" -> \"nvim\"");
    DotlangParser p;
    p.feed((tokens));
    p.parseMain();

    CHECK(p.link_files.size() == 1, "basic link: one entry in link_files");
    CHECK(p.sudo_link_files.empty(), "basic link: sudo_link_files stays empty");
}


void test_dir_copy_and_link() {
    auto tokens = lex_line("\"/etc/nginx\" >>* \"nginx\"");
    DotlangParser p;
    p.feed((tokens));
    p.parseMain();
    CHECK(p.copy_dirs.size() == 1, "dir copy: one entry in copy_dirs");

    auto tokens2 = lex_line("\"/etc/systemd\" ->* \"systemd\"");
    DotlangParser p2;
    p2.feed(tokens2);
    p2.parseMain();
    CHECK(p2.link_dirs.size() == 1, "dir link: one entry in link_dirs");
}


void test_sudo_action_routes_to_sudo_vector() {
    auto tokens = lex_line("#! allow-sudo");
    auto path_tokens = lex_line("@sudo \"/etc/hosts\" >> \"hosts\"");
    tokens.insert(tokens.end(), path_tokens.begin(), path_tokens.end());

    CHECK(!tokens.empty() && tokens[1].type == Token::ACTION, "sudo action: lexer emits ACTION token");
    CHECK(!tokens.empty() && tokens[1].name == "sudo", "sudo action: ACTION token carries 'sudo'");

    DotlangParser p;
    p.feed((tokens));
    auto report = p.parseMain();

    CHECK(p.sudo_copy_files.size() == 1, "sudo action: entry lands in sudo_copy_files");
    CHECK(p.copy_files.empty(), "sudo action: does NOT also land in copy_files");
}


void test_sudo_action_link_variant() {
    auto tokens = lex_line("#! allow-sudo");
    auto path_tokens = lex_line("@sudo \"/etc/passwd\" -> \"passwd\"");
    tokens.insert(tokens.end(), path_tokens.begin(), path_tokens.end());

    DotlangParser p;
    p.feed((tokens));
    p.parseMain();

    CHECK(p.sudo_link_files.size() == 1, "sudo action (link): entry lands in sudo_link_files");
    CHECK(p.link_files.empty(), "sudo action (link): does NOT also land in link_files");
}


void test_sudo_action_dir_variants() {
    auto tokens = lex_line("#! allow-sudo");
    auto path_tokens = lex_line("@sudo \"/etc/nginx\" >>* \"nginx\"");
    tokens.insert(tokens.end(), path_tokens.begin(), path_tokens.end());

    DotlangParser p;
    p.feed((tokens));
    p.parseMain();

    CHECK(p.sudo_copy_dirs.size() == 1, "sudo action (dir copy): entry lands in sudo_copy_dirs");
    CHECK(p.copy_dirs.empty(), "sudo action (dir copy): does NOT also land in copy_dirs");

    auto tokens2 = lex_line("#! allow-sudo");
    auto path_tokens2 = lex_line("@sudo \"/etc/systemd\" ->* \"systemd\"");
    tokens2.insert(tokens2.end(), path_tokens2.begin(), path_tokens2.end());

    DotlangParser p2;
    p2.feed((tokens2));
    p2.parseMain();

    CHECK(p2.sudo_link_dirs.size() == 1, "sudo action (dir link): entry lands in sudo_link_dirs");
    CHECK(p2.link_dirs.empty(), "sudo action (dir link): does NOT also land in link_dirs");
}


void test_sudo_without_directive_is_rejected() {
    // `@sudo` must NOT escalate unless `#! allow-sudo` was given for the file
    auto tokens = lex_line("@sudo \"/etc/hosts\" >> \"hosts\"");
    DotlangParser p;
    p.feed((tokens));
    auto report = p.parseMain();

    CHECK(report.error(), "sudo w/o directive: report flags an error");
    CHECK(p.sudo_copy_files.empty(), "sudo w/o directive: no entry in sudo_copy_files");
    CHECK(p.copy_files.empty(), "sudo w/o directive: no entry in copy_files either");
}


void test_unknown_action_reports_complain() {
    auto tokens = lex_line("@not-a-real-action \"/etc/hosts\" >> \"hosts\"");
    DotlangParser p;
    p.feed((tokens));
    auto report = p.parseMain();

    CHECK(report.error(), "unknown action: report flags an error");
    CHECK(p.copy_files.empty(), "unknown action: no entry in copy_files");
    CHECK(p.sudo_copy_files.empty(), "unknown action: no entry in sudo_copy_files");
}


void test_sudo_directive_alone_does_not_imply_action() {
    // `#! allow-sudo` sets the internal opts.sudo flag/mode, but WITHOUT
    // an explicit `@sudo` action token on a path line, that line
    // should NOT be treated as sudo (Action-token driven, not directive-driven).
    auto tokens = lex_line("#! allow-sudo");
    auto path_tokens = lex_line("\"/etc/hosts\" >> \"hosts\"");
    tokens.insert(tokens.end(), path_tokens.begin(), path_tokens.end());

    DotlangParser p;
    p.feed((tokens));
    p.parseMain();

    CHECK(p.copy_files.size() == 1, "directive alone: plain copy still recorded");
    CHECK(p.sudo_copy_files.empty(), "directive alone: sudo_copy_files stays empty without @sudo action");
}


void test_sudo_state_resets_between_parseMain_calls() {
    // enable flags persist across parseMain unless resetFileState() is called.
    // Vectors also accumulate unless cleared. resetFileState() clears both.
    DotlangParser p;

    auto tokens1 = lex_line("#! allow-sudo");
    auto path_tokens1 = lex_line("@sudo \"/etc/hosts\" >> \"hosts\"");
    tokens1.insert(tokens1.end(), path_tokens1.begin(), path_tokens1.end());
    p.feed((tokens1));
    p.parseMain();
    CHECK(p.sudo_copy_files.size() == 1, "reset check: first parseMain populates sudo_copy_files");

    // Without reset, enable flag still on — but plain mapping is not sudo
    auto tokens2 = lex_line("\"/etc/vimrc\" >> \"vimrc\"");
    p.feed((tokens2));
    p.parseMain();
    CHECK(p.copy_files.size() == 1, "reset check: non-sudo copy lands in copy_files");
    CHECK(p.sudo_copy_files.size() == 1, "reset check: sudo_copy_files unchanged by plain mapping");

    // Explicit reset clears vectors and disables actions
    p.resetFileState();
    CHECK(p.sudo_copy_files.empty(), "resetFileState clears sudo_copy_files");
    CHECK(p.copy_files.empty(), "resetFileState clears copy_files");

    auto tokens3 = lex_line("@sudo \"/etc/hosts\" >> \"hosts\"");
    p.feed(tokens3);
    auto report = p.parseMain();
    CHECK(report.error(), "after reset, @sudo without directive is rejected");
    CHECK(p.sudo_copy_files.empty(), "after reset, no sudo entry recorded");
}

void test_exec_action() {
    auto tokens = lex_line("#! allow-exec");
    auto exec_tokens = lex_line("@exec \'yay -S brave\'");
    tokens.insert(tokens.end(), exec_tokens.begin(), exec_tokens.end());

    DotlangParser p;
    p.feed(tokens);
    auto report = p.parseMain();
    CHECK(!report.error() || p.exec_commands.size() == 1, "exec: parse succeeds or records command");
    CHECK(p.exec_commands.size() == 1, "exec: one command recorded");
    if (!p.exec_commands.empty()) {
        CHECK(p.exec_commands[0] == "yay -S brave", "exec: command payload preserved");
    }
}

void test_exec_without_directive_is_rejected() {
    auto tokens = lex_line("@exec \'echo hello\'");
    DotlangParser p;
    p.feed(tokens);
    auto report = p.parseMain();
    CHECK(report.error(), "exec w/o directive: report flags error");
    CHECK(p.exec_commands.empty(), "exec w/o directive: no command recorded");
}

void test_single_quoted_string_lexes() {
    auto tokens = lex_line("@exec \'printf hi\'");
    CHECK(!tokens.empty(), "single-quote lex: produces tokens");
    bool has_string = false;
    for (auto& t : tokens) {
        if (t.type == Token::STRING && t.name == "printf hi") has_string = true;
    }
    CHECK(has_string, "single-quote lex: STRING token carries payload");
}


void test_unknown_directive_reports_complain() {
    auto tokens = lex_line("#! not-a-real-directive");
    DotlangParser p;
    p.feed((tokens));
    auto report = p.parseMain();

    CHECK(report.error(), "unknown directive: report flags an error");
}

void test_unterminated_string_is_rejected() {
    DotlangLexer lexer;
    lexer.feed(std::string{"\"/etc/hosts >> \"hosts\""});
    auto report = lexer.lexMain();
    CHECK(report.error(), "unterminated string: lexer reports an error");
}

void test_tabs_are_consumed_as_whitespace() {
    auto tokens = lex_line("\t\"/etc/hosts\"\t>>\t\"hosts\"");
    DotlangParser p;
    p.feed(tokens);
    p.parseMain();
    CHECK(p.copy_files.size() == 1, "tab whitespace: lexer progresses and parses mapping");
}


int main() {
    test_basic_copy();
    test_basic_link();
    test_dir_copy_and_link();
    test_sudo_action_routes_to_sudo_vector();
    test_sudo_action_link_variant();
    test_sudo_action_dir_variants();
    test_sudo_without_directive_is_rejected();
    test_unknown_action_reports_complain();
    test_sudo_directive_alone_does_not_imply_action();
    test_sudo_state_resets_between_parseMain_calls();
    test_unknown_directive_reports_complain();
    test_unterminated_string_is_rejected();
    test_tabs_are_consumed_as_whitespace();
    test_exec_action();
    test_exec_without_directive_is_rejected();
    test_single_quoted_string_lexes();

    if (failures) {
        std::cerr << "\n" << failures << " test(s) failed.\n";
        return 1;
    }
    std::cerr << "\nAll tests passed.\n";
}
