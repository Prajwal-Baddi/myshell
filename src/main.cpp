#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>



int g_lastStatus = 0;

// Simple environment variable and tilde expansion

struct Command;
bool runBuiltinWithRedirection(const Command& command);



std::string expandString(const std::string& input) {
    std::string result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '$') {
        if (i + 1 < input.size() && input[i+1] == '?') {
            result += std::to_string(g_lastStatus);
            ++i; // skip '?'
            continue;
        }
        // Handle ${VAR} or $VAR
        if (i + 1 < input.size() && input[i+1] == '{') {
                size_t end = input.find('}', i+2);
                if (end != std::string::npos) {
                    std::string varName = input.substr(i+2, end - (i+2));
                    const char* val = std::getenv(varName.c_str());
                    if (val) result += val;
                    i = end; // skip }
                    continue;
                }
            }
            // Simple $VAR (alphanum and _)
            size_t j = i+1;
            while (j < input.size() && (std::isalnum(static_cast<unsigned char>(input[j])) || input[j] == '_')) ++j;
            if (j > i+1) {
                std::string varName = input.substr(i+1, j-(i+1));
                const char* val = std::getenv(varName.c_str());
                if (val) result += val;
                i = j-1;
                continue;
            }
            // Lone $ stays as is
            result += '$';
        } else if (input[i] == '~' && (i == 0 || input[i-1] == ' ')) {
            const char* home = std::getenv("HOME");
            if (home) result += home;
            else result += '~';
        } else {
            result += input[i];
        }
    }
    return result;
}

// Expand all tokens that are not within single quotes (our tokenizer already stripped quotes)
std::vector<std::string> expandTokens(const std::vector<std::string>& tokens) {
    std::vector<std::string> out;
    out.reserve(tokens.size());
    for (const auto& t : tokens) {
        out.push_back(expandString(t));
    }
    return out;
}

#include "parser/tokenizer.h"
#include "parser/parser.h"
#include "builtins/builtins.h"
#include "executer/executer.h"
void initializeJobControl() {
    // Ignore signals that should affect foreground commands, not the shell itself.
    struct sigaction action {};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    const int signals[] = { SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU };
    for (int signalNumber : signals) {
        if (sigaction(signalNumber, &action, nullptr) == -1) {
            perror("myshell: sigaction");
        }
    }
    pid_t shellPid = getpid();
    if (setpgid(shellPid, shellPid) == -1 && getpgrp() != shellPid) {
        perror("myshell: setpgid");
    }
    if (isatty(STDIN_FILENO)) {
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
            perror("myshell: tcsetpgrp");
        }
    }
}





int executeSequence(const CommandSequence& sequence) {
    int lastStatus = 0;
    for (size_t idx = 0; idx < sequence.pipelines.size(); ++idx) {
        const Pipeline& pipeline = sequence.pipelines[idx];
        
        if (pipeline.commands.empty()) continue;

        if (idx > 0) {
            SequenceOp op = sequence.operators[idx - 1];
            if (op == SequenceOp::And && lastStatus != 0) {
                continue;
            }
            if (op == SequenceOp::Or && lastStatus == 0) {
                continue;
            }
        }

        const Command& command = pipeline.commands.front();
        if (pipeline.commands.size() == 1 && isBuiltin(command)) {
            bool ok = runBuiltinWithRedirection(command);
            lastStatus = ok ? 0 : 1;
        } else {
            lastStatus = executePipeline(pipeline);
        }
        g_lastStatus = lastStatus;
    }
    return lastStatus;
}

bool runBuiltinWithRedirection(const Command& command) {
    std::cout.flush();
    std::cerr.flush();

    int savedStdin = -1, savedStdout = -1, savedStderr = -1;
    if (!command.inputFile.empty()) {
        savedStdin = dup(STDIN_FILENO);
        int fd = open(command.inputFile.c_str(), O_RDONLY);
        if (fd == -1) { perror(command.inputFile.c_str()); return false; }
        if (dup2(fd, STDIN_FILENO) == -1) { perror("dup2"); close(fd); return false; }
        close(fd);
    }
    if (!command.outputFile.empty()) {
        savedStdout = dup(STDOUT_FILENO);
        int flags = O_WRONLY | O_CREAT | (command.outputAppend ? O_APPEND : O_TRUNC);
        int fd = open(command.outputFile.c_str(), flags, 0644);
        if (fd == -1) { perror(command.outputFile.c_str()); return false; }
        if (dup2(fd, STDOUT_FILENO) == -1) { perror("dup2"); close(fd); return false; }
        close(fd);
    }
    if (!command.errorFile.empty()) {
        savedStderr = dup(STDERR_FILENO);
        int flags = O_WRONLY | O_CREAT | (command.errorAppend ? O_APPEND : O_TRUNC);
        int fd = open(command.errorFile.c_str(), flags, 0644);
        if (fd == -1) { perror(command.errorFile.c_str()); return false; }
        if (dup2(fd, STDERR_FILENO) == -1) { perror("dup2"); close(fd); return false; }
        close(fd);
    }
    if (command.errorToStdout) {
        savedStderr = dup(STDERR_FILENO);
        if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) { perror("dup2"); return false; }
    }
    bool ok = executeBuiltin(command);
    if (savedStdin != -1) { dup2(savedStdin, STDIN_FILENO); close(savedStdin); }
    if (savedStdout != -1) { dup2(savedStdout, STDOUT_FILENO); close(savedStdout); }
    if (savedStderr != -1) { dup2(savedStderr, STDERR_FILENO); close(savedStderr); }
    return ok;
}

    int main() {
        initializeJobControl();
        installChildHandler();
        
        // Set a custom home directory for the shell's ~ expansion and prompt
        std::string shellHome = "/run/media/jarvis/Windows/Users/hrish/Downloads/Learn/Projects/myshell";
        setenv("HOME", shellHome.c_str(), 1);

        while (true) {
            char cwd[PATH_MAX];
            std::string prompt = "";
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                std::string currentDir = cwd;
                const char* home = std::getenv("HOME");
                if (home && currentDir.find(home) == 0) {
                    currentDir.replace(0, std::string(home).length(), "~");
                }
                prompt = currentDir;
            }
            prompt += "> ";

            std::string input;
            char* line = readline(prompt.c_str());
            if (!line) {
                std::cout << '\n';
                break;
            }
            input = line;
            free(line);
            if (!input.empty()) {
                add_history(input.c_str());
            }



            reapBackgroundChildren();
            
            std::vector<std::string> tokens = tokenize(input);
            tokens = expandTokens(tokens);




        if (tokens.empty()) {
            continue;
        }

        // If tokens contain any sequencing operators, handle them specially.
        bool hasSeq = false;
        for (const auto& t : tokens) {
            if (t == ";" || t == "&&" || t == "||") {
                hasSeq = true;
                break;
            }
        }
        int status = 0;
        if (hasSeq) {
            try {
                CommandSequence sequence = parseSequence(tokens);
                status = executeSequence(sequence);
            } catch (const std::runtime_error& e) {
                std::cerr << "Parse error: " << e.what() << '\n';
                continue;
            }
        } else {
            Pipeline pipeline;
            try {
                pipeline = parse(tokens);
            } catch (const std::runtime_error& error) {
                std::cerr << "Parse error: " << error.what() << '\n';
                continue;
            }

            const Command& command = pipeline.commands.front();

            if (pipeline.commands.size() == 1 && isBuiltin(command)) {
                if (!runBuiltinWithRedirection(command)) {
                    std::cerr << "Error executing builtin command: "
                              << command.program << '\n';
                }
                status = 0; // builtin errors already printed
                g_lastStatus = status;
            } else {
                status = executePipeline(pipeline);
                g_lastStatus = status;
            }
        }
        // $? could be stored globally if needed later.
        (void)status; // currently unused
    }
}

