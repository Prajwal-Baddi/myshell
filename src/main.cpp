#include <iostream>
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

        Command command = parse(tokens);

        if (isBuiltin(command)) {
            if (!executeBuiltin(command)) {
                std::cerr << "Error executing builtin command: " << command.program << '\n';
            }
      } else {
    executeCommand(command);
}
    }
}
