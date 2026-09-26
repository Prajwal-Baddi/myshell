#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <vector>

struct Command {
    std::string program;
    std::vector<std::string> arguments;
};

#endif