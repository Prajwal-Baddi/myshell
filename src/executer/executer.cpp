#include "executer.h"

#include <cerrno>
#include <cstdio>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>  
#include <array>
#include <string>

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

int executePipeline(const Pipeline& pipeline) {
    const size_t commandCount = pipeline.commands.size();

    if (commandCount == 0) {
        return 0;
    }

    // N commands need N - 1 pipes.
    std::vector<std::array<int, 2>> pipes(commandCount - 1);

    // Mark every end as unopened so cleanup can distinguish it from fd 0.
    for (auto& pipeFds : pipes) {
        pipeFds = {-1, -1};
    }

    for (auto& pipeFds : pipes) {
        if (pipe(pipeFds.data()) == -1) {
            perror("pipe");

            // Close any pipes already created.
            for (auto& openedPipe : pipes) {
                if (openedPipe[0] != -1) close(openedPipe[0]);
                if (openedPipe[1] != -1) close(openedPipe[1]);
            }
            return 1;
        }
    }

    std::vector<pid_t> pids;
    bool forkFailed = false;

    for (size_t i = 0; i < commandCount; ++i) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            forkFailed = true;
            break;
        }

        if (pid == 0) {
            const Command& command = pipeline.commands[i];

            // Read from the previous command's pipe.
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }

            // Write to the next command's pipe.
            if (i + 1 < commandCount &&
                dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }

            // The child no longer needs the original pipe descriptors.
            for (const auto& pipeFds : pipes) {
                close(pipeFds[0]);
                close(pipeFds[1]);
            }

            // Apply file redirections after pipe connections.
            if (!command.inputFile.empty()) {
                int fd = open(command.inputFile.c_str(), O_RDONLY);
                if (fd == -1) {
                    perror(command.inputFile.c_str());
                    _exit(1);
                }

                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2");
                    close(fd);
                    _exit(1);
                }
                if (fd != STDIN_FILENO) close(fd);
            }

            if (!command.outputFile.empty()) {
                int flags = O_WRONLY | O_CREAT;
                flags |= command.append ? O_APPEND : O_TRUNC;

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
                if (fd != STDOUT_FILENO) close(fd);
            }

            // Build the argument list for execvp.
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

            execvp(argv[0], argv.data());

            perror(argv[0]);
            _exit(errno == ENOENT ? 127 : 126);
        }

        pids.push_back(pid);
    }

    // The parent must close its pipe ends too, or readers may never see EOF.
    for (const auto& pipeFds : pipes) {
        close(pipeFds[0]);
        close(pipeFds[1]);
    }

    // Wait for every child. Return the last command's status.
    int lastStatus = 1;

    for (size_t i = 0; i < pids.size(); ++i) {
        int status;
        pid_t result;

        do {
            result = waitpid(pids[i], &status, 0);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            perror("waitpid");
            continue;
        }

        if (i == pids.size() - 1) {
            if (WIFEXITED(status)) {
                lastStatus = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                lastStatus = 128 + WTERMSIG(status);
            }
        }
    }

    if (forkFailed) {
        return 1;
    }

    return lastStatus;
}
