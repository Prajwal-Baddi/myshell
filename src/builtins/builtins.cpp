#include "builtins.h"

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <limits.h>
#include <csignal>
#include <unordered_map>

#include "../executer/executer.h"

namespace {
bool parseJobId(const std::string& argument, int& jobId) {
    if (argument.size() < 2 || argument[0] != '%') {
        return false;
    }

    try {
        size_t parsedLength = 0;
        int parsedId = std::stoi(argument.substr(1), &parsedLength);

        if (parsedLength != argument.size() - 1 || parsedId <= 0) {
            return false;
        }

        jobId = parsedId;
        return true;
    } catch (...) {
        return false;
    }
}

bool executeJobControl(const Command& command, bool foreground) {
    const char* commandName = foreground ? "fg" : "bg";

    if (command.arguments.size() > 1) {
        std::cerr << "Usage: " << commandName << " [%job_id]\n";
        return false;
    }

    int jobId = -1;
    if (!command.arguments.empty() &&
        !parseJobId(command.arguments[0], jobId)) {
        std::cerr << "Usage: " << commandName << " [%job_id]\n";
        return false;
    }

    return foreground
        ? resumeJobInForeground(jobId)
        : resumeJobInBackground(jobId);
}

// Simple environment variable handling for export/unset
void setEnv(const std::string& name, const std::string& value) {
    setenv(name.c_str(), value.c_str(), 1);
}

void unsetEnv(const std::string& name) {
    unsetenv(name.c_str());
}
}

bool isBuiltin(const Command& command) {
    return command.program == "cd" ||
           command.program == "pwd" ||
           command.program == "exit" ||
           command.program == "jobs" ||
           command.program == "bg" ||
           command.program == "fg" ||
           command.program == "kill" ||
           command.program == "export" ||
           command.program == "unset" ||
           command.program == "echo" ||
           command.program == "help";
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

    if (command.program == "pwd") {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::cout << cwd << '\n';
            return true;
        }
        perror("pwd");
        return false;
    }

    if (command.program == "jobs") {
        listJobs();
        return true;
    }

    if (command.program == "bg") {
        return executeJobControl(command, false);
    }

    if (command.program == "fg") {
        return executeJobControl(command, true);
    }

    if (command.program == "kill") {
        // Existing kill implementation unchanged (omitted for brevity)
        // ... (copy previous kill handling code) ...
        // For brevity, we will call the previous implementation by delegating.
        // We'll reuse the existing code by re-including the original block.
        // NOTE: The original code is inserted below unchanged.
        if (command.arguments.empty() ||
            command.arguments.size() > 2) {
            std::cerr << "Usage: kill [-signal] %job_id\n";
            return false;
        }

        int signalNumber = SIGTERM;
        std::string jobArgument;

        if (command.arguments.size() == 2) {
            std::string signalArgument = command.arguments[0];
            if (signalArgument.size() < 2 ||
                signalArgument[0] != '-') {
                std::cerr << "Usage: kill [-signal] %job_id\n";
                return false;
            }
            try {
                signalNumber = std::stoi(signalArgument.substr(1));
            } catch (...) {
                std::cerr << "kill: invalid signal\n";
                return false;
            }
            jobArgument = command.arguments[1];
        } else {
            jobArgument = command.arguments[0];
        }

        if (jobArgument.size() < 2 ||
            jobArgument[0] != '%') {
            std::cerr << "Usage: kill [-signal] %job_id\n";
            return false;
        }

        int jobId;
        try {
            size_t parsedLength = 0;
            jobId = std::stoi(jobArgument.substr(1), &parsedLength);
            if (parsedLength != jobArgument.size() - 1 ||
                jobId <= 0) {
                std::cerr << "kill: invalid job ID\n";
                return false;
            }
        } catch (...) {
            std::cerr << "kill: invalid job ID\n";
            return false;
        }

        if (signalNumber <= 0 || signalNumber >= NSIG) {
            std::cerr << "kill: invalid signal number\n";
            return false;
        }

        return killJob(jobId, signalNumber);
    }

    if (command.program == "export") {
        if (command.arguments.empty()) {
            // Print all exported variables (environment)
            extern char** environ;
            for (char** env = environ; *env != nullptr; ++env) {
                std::cout << *env << '\n';
            }
            return true;
        }
        for (const auto& arg : command.arguments) {
            size_t eqPos = arg.find('=');
            if (eqPos == std::string::npos) {
                // export NAME – mark existing env var for export (no effect in this simple shell)
                const char* val = std::getenv(arg.c_str());
                if (val) setenv(arg.c_str(), val, 1);
                continue;
            }
            std::string name = arg.substr(0, eqPos);
            std::string value = arg.substr(eqPos + 1);
            setEnv(name, value);
        }
        return true;
    }

    if (command.program == "unset") {
        for (const auto& arg : command.arguments) {
            unsetEnv(arg);
        }
        return true;
    }

    if (command.program == "echo") {
        bool noNewline = false;
        size_t startIdx = 0;
        if (!command.arguments.empty() && command.arguments[0] == "-n") {
            noNewline = true;
            startIdx = 1;
        }
        for (size_t i = startIdx; i < command.arguments.size(); ++i) {
            if (i > startIdx) std::cout << ' ';
            std::cout << command.arguments[i];
        }
        if (!noNewline) std::cout << '\n';
        return true;
    }

    if (command.program == "help") {
        std::cout << "myshell builtins:\n";
        std::cout << "  cd [dir]        – change directory (default $HOME)\n";
        std::cout << "  pwd              – print current working directory\n";
        std::cout << "  exit             – exit the shell\n";
        std::cout << "  jobs             – list background jobs\n";
        std::cout << "  fg [%id]         – bring job to foreground\n";
        std::cout << "  bg [%id]         – resume stopped job in background\n";
        std::cout << "  kill [-sig] %id  – send signal to job\n";
        std::cout << "  export [VAR=VAL] – set or list environment variables\n";
        std::cout << "  unset VAR        – remove environment variable\n";
        std::cout << "  echo [-n] args   – print arguments\n";
        std::cout << "  help             – this help message\n";
        return true;
    }

    return false;
}
