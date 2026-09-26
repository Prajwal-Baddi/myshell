#include <iostream>
#include <string>
#include <vector>
#include "parser/tokenizer.h"

int main() {
    while (true) {
        std::cout << "myshell> ";

        std::string s;
        std::getline(std::cin, s);

        std::vector<std::string> tokens = tokenize(s);

        if (tokens.empty()) {
            continue;
        }

        for (const auto& token : tokens) {
            std::cout << "Token: [" << token << "]\n";
        }
    }
}