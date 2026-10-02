#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "parser/tokenizer.h"
#include "parser/parser.h"
#include "builtins/builtins.h"
#include "executer/executer.h"

int main() {
    while (true) {
        std::cout << "myshell> ";

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << '\n';
            break;
        }

        std::vector<std::string> tokens = tokenize(input);

        if (tokens.empty()) {
            continue;
        }

        Command command;
        try {
            command = parse(tokens);
        } catch (const std::runtime_error& error) {
            std::cerr << "Parse error: " << error.what() << '\n';
            continue;
        }

        if (isBuiltin(command)) {
            if (!executeBuiltin(command)) {
                std::cerr << "Error executing builtin command: " << command.program << '\n';
            }
      } else {
    executeCommand(command);
}
    }
}
