#include "executer.h"

#include <cerrno>
#include <cstdio>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>  // open() and file-opening flags

int executeCommand(const Command& command) {
    if (command.program.empty()) {
        return 0;
    }

    // Make writable copies for execvp's argument list.
    std::vector<std::string> words;
    words.push_back(command.program);

    for (const auto& argument : command.arguments) {
        words.push_back(argument);
    }

    std::vector<char*> argv;

    for (auto& word : words) {
        argv.push_back(word.data());
    }

    argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

        if (pid == 0) {
        // Input redirection: <
        if (!command.inputFile.empty()) {
            int inputFd = open(command.inputFile.c_str(), O_RDONLY);

            if (inputFd == -1) {
                perror(command.inputFile.c_str());
                _exit(1);
            }

            if (dup2(inputFd, STDIN_FILENO) == -1) {
                perror("dup2");
                close(inputFd);
                _exit(1);
            }

            if (inputFd != STDIN_FILENO) {
                close(inputFd);
            }
        }

        // Output redirection: > or >>
        if (!command.outputFile.empty()) {
            int flags = O_WRONLY | O_CREAT;

            if (command.append) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }

            int fd = open(command.outputFile.c_str(), flags, 0644);

            if (fd == -1) {
                perror(command.outputFile.c_str());
                _exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                _exit(1);
            }

            if (fd != STDOUT_FILENO) {
                close(fd);
            }
        }

        execvp(argv[0], argv.data());

        int error = errno;
        perror(argv[0]);
        _exit(error == ENOENT ? 127 : 126);
    }
    int status;

    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return 1;
}
