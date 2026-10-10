#include "tokenizer.h"
#include <cctype>

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;

    auto flush = [&]() {
        if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    };

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '"' || c == '\'') {
            for (++i; i < input.size() && input[i] != c; ++i) {
                token += input[i];
            }
        } else if (c == '|') {
            flush();
            tokens.push_back("|");
        } else if (isspace(static_cast<unsigned char>(c))) {
            flush();
        } else {
            token += c;
        }
    }

    flush();

    return tokens;
}
