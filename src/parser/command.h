#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <vector>

struct Command {
    std::string program;
    std::vector<std::string> arguments;
    std::string inputFile;               // stdin redirection
    std::string outputFile;               // stdout redirection (truncate)
    bool outputAppend = false;           // stdout append mode
    std::string errorFile;                // stderr redirection (truncate)
    bool errorAppend = false;            // stderr append mode
    bool errorToStdout = false;          // 2>&1 redirection flag
};

enum class SequenceOp {
    None,
    Semicolon,
    And,
    Or
};

struct Pipeline {
    std::vector<Command> commands;
    bool background = false;
};

struct CommandSequence {
    std::vector<Pipeline> pipelines;
    std::vector<SequenceOp> operators;
};

#endif
