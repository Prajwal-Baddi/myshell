#include "parser.h"

Command parse(const std::vector<std::string>& tokens) {
    Command command;

    if (tokens.empty()) {
        return command;
    }

    command.program = tokens[0];

    for (size_t i = 1; i < tokens.size(); ++i) {
        command.arguments.push_back(tokens[i]);
    }

    return command;
}