#include "parser.h"
#include <stdexcept>

Command parse(const std::vector<std::string>& tokens) {
    Command command;

    if (tokens.empty()) {
        return command;
    }

    command.program = tokens[0];

  for (size_t i = 1; i < tokens.size(); ++i) {
   if (tokens[i] == ">" || tokens[i] == ">>") {
    if (i + 1 >= tokens.size() ||
        tokens[i + 1] == ">" ||
        tokens[i + 1] == ">>"||
        tokens[i + 1] == "<") {
        throw std::runtime_error(
            "expected a filename after " + tokens[i]
        );
    }

    command.append = (tokens[i] == ">>");
    command.outputFile = tokens[++i];
} else if (tokens[i] == "<") {
    if (i + 1 >= tokens.size() ||
        tokens[i + 1] == "<" ||
        tokens[i + 1] == ">" ||
        tokens[i + 1] == ">>") {
        throw std::runtime_error("expected a filename after <");
    }

    command.inputFile = tokens[++i];
} else {
    command.arguments.push_back(tokens[i]);
}
}

    return command;
}
