#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <vector>

struct Command {
    std::string program;
    std::vector<std::string> arguments;
    std::string outputFile;  // Empty means normal terminal output.
    bool append = false;
    std::string inputFile;
};
#endif