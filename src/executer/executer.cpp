#include "executer.h"

#include <cerrno>
#include <cstdio>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

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
        execvp(argv[0], argv.data());

        // Reached only if execution fails.
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
