#include "builtins.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <limits.h>

bool isBuiltin(const Command& command) {
    return command.program == "cd" ||
           command.program == "exit" ||
           command.program == "pwd";
}

bool executeBuiltin(const Command& command) {
    if (command.program == "cd") {

        if (command.arguments.empty()) {
            const char* home = std::getenv("HOME");

            if (home == nullptr) {
                std::cerr << "cd: HOME is not set\n";
                return false;
            }

            if (chdir(home) != 0) {
                perror("cd");
                return false;
            }

            return true;
        }

        const std::string& path = command.arguments[0];

        if (chdir(path.c_str()) != 0) {
            perror("cd");
            return false;
        }

        return true;
    }

    if (command.program == "exit") {
        std::exit(0);
    }

    if(command.program == "pwd") {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::cout << cwd << '\n';
            return true;
        } else {
            perror("pwd");
            return false;
        }
    }

    return false;
}