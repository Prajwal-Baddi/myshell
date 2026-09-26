#include <iostream>
#include <string>
#include <vector>

#include "parser/tokenizer.h"
#include "parser/parser.h"

int main() {
    while (true) {
        std::cout << "myshell> ";

        std::string input;
        std::getline(std::cin, input);

        std::vector<std::string> tokens = tokenize(input);

        if (tokens.empty()) {
            continue;
        }

        Command command = parse(tokens);

        std::cout << "Program: " << command.program << '\n';

        for (const auto& arg : command.arguments) {
            std::cout << "Argument: " << arg << '\n';
        }
    }
}