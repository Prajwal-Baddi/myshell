#include "tokenizer.h"
#include <cctype>
#include <stdexcept>

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    auto flush = [&]() {
        if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    };

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        // Handle escape character
        if (c == '\\' && !(inSingleQuote)) {
            // In double quotes or unquoted, escape next character
            if (i + 1 < input.size()) {
                token += input[++i];
            }
            continue;
        }
        // Quote handling
        if (c == '\'') {
            if (!inDoubleQuote) {
                inSingleQuote = !inSingleQuote;
                continue; // quotes are not part of token
            }
        } else if (c == '"') {
            if (!inSingleQuote) {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }
        }
        // If we are inside a quote, take everything literally (except escaped handled above)
        if (inSingleQuote || inDoubleQuote) {
            token += c;
            continue;
        }
        // Check for multi-char operators (including redirection combos)
        if (i + 1 < input.size()) {
            std::string twoChars = input.substr(i, 2);
            if (twoChars == "&&" || twoChars == "||" || twoChars == ">>" ||
                twoChars == "2>" || twoChars == "2>>" || twoChars == "2>&") {
                // Handle "2>&1"
                if (twoChars == "2>&" && i + 2 < input.size() && input[i+2] == '1') {
                    flush();
                    tokens.push_back("2>&1");
                    i += 2; // consume '&' and '1'
                    continue;
                }
                flush();
                tokens.push_back(twoChars);
                i += 1; // consume extra character
                continue;
            }
        }
        // Single-char operators
        if (c == '|' || c == '<' || c == '>' || c == ';' || c == '&') {
            flush();
            tokens.emplace_back(1, c);
            continue;
        }
        // Whitespace ends token
        if (isspace(static_cast<unsigned char>(c))) {
            flush();
            continue;
        }
        // Regular character
        token += c;
    }
    
    // Detect unterminated quotes
    if (inSingleQuote || inDoubleQuote) {
        throw std::runtime_error("unterminated quote in input");
    }
    
    flush();
    return tokens;
}


