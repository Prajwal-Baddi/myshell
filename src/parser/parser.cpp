#include "parser.h"
#include <stdexcept>

namespace {

Command parseCommand(const std::vector<std::string>& tokens) {
    Command command;

    if (tokens.empty()) {
        throw std::runtime_error("expected a command");
    }

    if (tokens[0] == "<" || tokens[0] == ">" || tokens[0] == ">>") {
        throw std::runtime_error("expected a command before redirection");
    }

    command.program = tokens[0];

    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == ">" || tokens[i] == ">>") {
            if (i + 1 >= tokens.size() ||
                tokens[i + 1] == "<" ||
                tokens[i + 1] == ">" ||
                tokens[i + 1] == ">>") {
                throw std::runtime_error("expected a filename after " + tokens[i]);
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

} // namespace

Pipeline parse(const std::vector<std::string>& tokens) {
    Pipeline pipeline;

    if (tokens.empty()) {
        return pipeline;
    }

    std::vector<std::string> commandTokens;

    for (const auto& token : tokens) {
        if (token == "|") {
            if (commandTokens.empty()) {
                throw std::runtime_error("expected a command before |");
            }

            pipeline.commands.push_back(parseCommand(commandTokens));
            commandTokens.clear();
        } else {
            commandTokens.push_back(token);
        }
    }

    if (commandTokens.empty()) {
        throw std::runtime_error("expected a command after |");
    }

    pipeline.commands.push_back(parseCommand(commandTokens));
    return pipeline;
}
