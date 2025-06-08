#pragma once
#include "parser/Parser.hpp"

namespace aleph3 {

    class ParserTestHelper {
    public:
        static std::string parse_identifier(Parser& parser) {
            return parser.parse_identifier();
        }
        static ExprPtr parse_symbol(Parser& parser) {
            return parser.parse_symbol();
        }
        static bool match(Parser& parser, char expected) {
            return parser.match(expected);
        }
    };

} // namespace aleph3