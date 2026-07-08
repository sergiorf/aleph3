#include "frontend/Parser.hpp"

#include "syntax/Parser.hpp"
#include "syntax/TrustedSubsetLowering.hpp"

#include <string>
#include <utility>

namespace aleph3::frontend {

namespace {

std::string frontend_code_for(std::string code) {
    constexpr std::string_view syntax_parser_prefix = "syntax.parser.";
    constexpr std::string_view syntax_lexer_prefix = "syntax.lexer.";

    if (code.starts_with(syntax_parser_prefix)) {
        return "frontend.parser." + code.substr(syntax_parser_prefix.size());
    }
    if (code.starts_with(syntax_lexer_prefix)) {
        return "frontend.lexer." + code.substr(syntax_lexer_prefix.size());
    }
    return code;
}

void rewrite_frontend_codes(std::vector<Diagnostic>& diagnostics) {
    for (auto& diagnostic : diagnostics) {
        diagnostic.code = frontend_code_for(std::move(diagnostic.code));
    }
}

}  // namespace

Parser::Parser(std::string_view source) : source_(source) {}

ParseResult Parser::parse() const {
    syntax::Parser parser(source_);
    auto syntax_result = parser.parse();

    ParseResult result;
    if (!syntax_result.ok()) {
        result.diagnostics = std::move(syntax_result.diagnostics);
        rewrite_frontend_codes(result.diagnostics);
        return result;
    }

    auto lowered = syntax::lower_to_trusted_ir(syntax_result.root);
    result.root = std::move(lowered.root);
    result.diagnostics = std::move(lowered.diagnostics);
    rewrite_frontend_codes(result.diagnostics);
    return result;
}

}  // namespace aleph3::frontend
