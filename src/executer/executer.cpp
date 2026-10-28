#include "executer.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t childChanged = 0;

void handleChildSignal(int) {
    childChanged = 1;
}

void restoreChildSignals() {
    struct sigaction action {};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);

    const int signals[] = {
        SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD
    };

    for (int signalNumber : signals) {
        if (sigaction(signalNumber, &action, nullptr) == -1) {
            perror("myshell: child sigaction");
            _exit(1);
        }
    }
}

void closePipes(std::vector<std::array<int, 2>>& pipes) {
    for (auto& pipeFds : pipes) {
        if (pipeFds[0] != -1) {
            close(pipeFds[0]);
            pipeFds[0] = -1;
        }
        if (pipeFds[1] != -1) {
            close(pipeFds[1]);
            pipeFds[1] = -1;
        }
    }
}

enum class JobState {
    Running,
    Stopped,
    Done
};

struct Job {
    int id;
    pid_t pgid;
    // Children that have not yet been collected as exited.
    std::vector<pid_t> pids;
    // The subset of pids currently reported stopped.
    std::vector<pid_t> stoppedPids;
    std::string command;
    JobState state;
};

std::vector<Job> jobs;
int nextJobId = 1;

std::string getJobStateName(JobState state) {
    switch (state) {
        case JobState::Running: return "Running";
        case JobState::Stopped: return "Stopped";
        case JobState::Done: return "Done";
    }
    return "Unknown";
}

std::string describePipeline(const Pipeline& pipeline) {
    std::ostringstream output;

    for (size_t i = 0; i < pipeline.commands.size(); ++i) {
        if (i > 0) {
            output << " | ";
        }

        const Command& command = pipeline.commands[i];
        output << command.program;

        for (const auto& argument : command.arguments) {
            output << ' ' << argument;
        }

        if (!command.inputFile.empty()) {
            output << " < " << command.inputFile;
        }
        if (!command.outputFile.empty()) {
            output << (command.outputAppend ? " >> " : " > ")
                   << command.outputFile;
        }
    }

    if (pipeline.background) {
        output << " &";
    }

    return output.str();
}

void addJob(
    pid_t pgid,
    const std::vector<pid_t>& pids,
    const std::vector<pid_t>& stoppedPids,
    const std::string& command,
    JobState state
) {
    jobs.push_back({
        nextJobId++,
        pgid,
        pids,
        stoppedPids,
        command,
        state
    });
}

void refreshJobState(Job& job) {
    if (job.pids.empty()) {
        job.state = JobState::Done;
        job.stoppedPids.clear();
        return;
    }

    // Remove stopped markers for children no longer tracked.
    job.stoppedPids.erase(
        std::remove_if(
            job.stoppedPids.begin(),
            job.stoppedPids.end(),
            [&job](pid_t pid) {
                return std::find(job.pids.begin(), job.pids.end(), pid)
                    == job.pids.end();
            }
        ),
        job.stoppedPids.end()
    );

    job.state = (!job.stoppedPids.empty() &&
                 job.stoppedPids.size() == job.pids.size())
        ? JobState::Stopped
        : JobState::Running;
}

void updateReapedChild(pid_t pid) {
    for (auto& job : jobs) {
        auto it = std::find(job.pids.begin(), job.pids.end(), pid);
        if (it == job.pids.end()) {
            continue;
        }

        job.pids.erase(it);
        job.stoppedPids.erase(
            std::remove(job.stoppedPids.begin(), job.stoppedPids.end(), pid),
            job.stoppedPids.end()
        );
        refreshJobState(job);
        return;
    }
}

void updateStoppedChild(pid_t pid) {
    for (auto& job : jobs) {
        if (std::find(job.pids.begin(), job.pids.end(), pid) == job.pids.end()) {
            continue;
        }

        if (std::find(job.stoppedPids.begin(), job.stoppedPids.end(), pid)
            == job.stoppedPids.end()) {
            job.stoppedPids.push_back(pid);
        }

        refreshJobState(job);
        return;
    }
}

void updateContinuedChild(pid_t pid) {
    for (auto& job : jobs) {
        if (std::find(job.pids.begin(), job.pids.end(), pid) == job.pids.end()) {
            continue;
        }

        job.stoppedPids.erase(
            std::remove(job.stoppedPids.begin(), job.stoppedPids.end(), pid),
            job.stoppedPids.end()
        );
        refreshJobState(job);
        return;
    }
}

Job* findJobById(int jobId) {
    for (auto& job : jobs) {
        if (job.id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

// Wait for all live members of one process group until the whole remaining
// job stops or all its children exit. This avoids waiting for pipeline PIDs
// sequentially, which can hang when one member stops before another.
bool waitForProcessGroup(
    pid_t pgid,
    std::vector<pid_t>& livePids,
    std::vector<pid_t>& stoppedPids,
    pid_t lastPid,
    int& lastStatus,
    bool& jobStopped
) {
    jobStopped = false;

    while (!livePids.empty()) {
        if (!stoppedPids.empty() && stoppedPids.size() == livePids.size()) {
            jobStopped = true;
            return true;
        }

        int status = 0;
        pid_t result;
        do {
            result = waitpid(-pgid, &status, WUNTRACED | WCONTINUED);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            if (errno == ECHILD) {
                // A child may have been collected elsewhere. Drop the group
                // from this local wait set; the caller will reconcile state.
                livePids.clear();
                stoppedPids.clear();
                return true;
            }

            perror("waitpid");
            return false;
        }

        auto liveIt = std::find(livePids.begin(), livePids.end(), result);

        if (WIFSTOPPED(status)) {
            if (liveIt != livePids.end() &&
                std::find(stoppedPids.begin(), stoppedPids.end(), result)
                    == stoppedPids.end()) {
                stoppedPids.push_back(result);
            }

            if (result == lastPid) {
                lastStatus = 128 + WSTOPSIG(status);
            }
        } else if (WIFCONTINUED(status)) {
            stoppedPids.erase(
                std::remove(stoppedPids.begin(), stoppedPids.end(), result),
                stoppedPids.end()
            );
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (liveIt != livePids.end()) {
                livePids.erase(liveIt);
            }
            stoppedPids.erase(
                std::remove(stoppedPids.begin(), stoppedPids.end(), result),
                stoppedPids.end()
            );

            if (result == lastPid) {
                lastStatus = WIFEXITED(status)
                    ? WEXITSTATUS(status)
                    : 128 + WTERMSIG(status);
            }
        }
    }

    return true;
}

} // namespace

void installChildHandler() {
    struct sigaction action {};
    action.sa_handler = handleChildSignal;
    sigemptyset(&action.sa_mask);
    // Do not use SA_NOCLDSTOP: stop/continue changes should wake the shell.
    action.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &action, nullptr) == -1) {
        perror("myshell: sigaction");
    }
}

void reapBackgroundChildren() {
    childChanged = 0;

    while (true) {
        int status = 0;
        pid_t pid = waitpid(
            -1,
            &status,
            WNOHANG | WUNTRACED | WCONTINUED
        );

        if (pid > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                updateReapedChild(pid);
            } else if (WIFSTOPPED(status)) {
                updateStoppedChild(pid);
            } else if (WIFCONTINUED(status)) {
                updateContinuedChild(pid);
            }
            continue;
        }

        if (pid == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        // ECHILD is normal when there are no waitable children.
        break;
    }
}

void listJobs() {
    reapBackgroundChildren();

    for (const auto& job : jobs) {
        std::cout << '[' << job.id << "] "
                  << getJobStateName(job.state) << '\t'
                  << job.command << '\n';
    }
}

bool resumeJobInBackground(int jobId) {
    reapBackgroundChildren();

    Job* selectedJob = nullptr;

    if (jobId == -1) {
        for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
            if (it->state == JobState::Stopped) {
                selectedJob = &(*it);
                break;
            }
        }
        if (selectedJob == nullptr) {
            std::cerr << "bg: no stopped job\n";
            return false;
        }
    } else {
        selectedJob = findJobById(jobId);
        if (selectedJob == nullptr || selectedJob->state == JobState::Done) {
            std::cerr << "bg: job %" << jobId << " not found\n";
            return false;
        }
        if (selectedJob->state != JobState::Stopped) {
            std::cerr << "bg: job %" << jobId << " is not stopped\n";
            return false;
        }
    }

    if (kill(-selectedJob->pgid, SIGCONT) == -1) {
        perror("bg: SIGCONT");
        return false;
    }

    selectedJob->stoppedPids.clear();
    selectedJob->state = JobState::Running;

    std::cout << '[' << selectedJob->id << "] "
              << selectedJob->command << " &\n";
    return true;
}

bool resumeJobInForeground(int jobId) {
    reapBackgroundChildren();

    Job* selectedJob = nullptr;
    if (jobId == -1) {
        for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
            if (it->state != JobState::Done) {
                selectedJob = &(*it);
                break;
            }
        }
    } else {
        selectedJob = findJobById(jobId);
    }

    if (selectedJob == nullptr || selectedJob->state == JobState::Done) {
        std::cerr << "fg: "
                  << (jobId == -1 ? "no current job\n"
                                  : "job %" + std::to_string(jobId) + " not found\n");
        return false;
    }

    const int selectedId = selectedJob->id;
    const pid_t pgid = selectedJob->pgid;
    const std::string commandText = selectedJob->command;

    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, pgid) == -1) {
        perror("fg: tcsetpgrp");
        return false;
    }

    if (selectedJob->state == JobState::Stopped) {
        if (kill(-pgid, SIGCONT) == -1) {
            perror("fg: SIGCONT");
            if (isatty(STDIN_FILENO)) {
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
            return false;
        }
        selectedJob->stoppedPids.clear();
        selectedJob->state = JobState::Running;
    }

    std::cout << commandText << '\n';

    int lastStatus = 0;
    bool jobStopped = false;
    std::vector<pid_t> livePids = selectedJob->pids;
    std::vector<pid_t> stoppedPids = selectedJob->stoppedPids;

    const pid_t lastPid = livePids.empty() ? -1 : livePids.back();
    bool waitOk = waitForProcessGroup(
        pgid, livePids, stoppedPids, lastPid, lastStatus, jobStopped
    );

    // Reconcile the persistent job with the wait results.
    selectedJob->pids = livePids;
    selectedJob->stoppedPids = stoppedPids;
    refreshJobState(*selectedJob);

    if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("fg: restore terminal");
    }

    if (jobStopped) {
        selectedJob->state = JobState::Stopped;
        std::cout << '[' << selectedId << "] Stopped\t"
                  << commandText << '\n';
    } else {
        refreshJobState(*selectedJob);
    }

    return waitOk;
}

bool killJob(int jobId, int signalNumber) {
    reapBackgroundChildren();

    Job* job = findJobById(jobId);
    if (job == nullptr || job->state == JobState::Done) {
        std::cerr << "kill: no such active job: %" << jobId << '\n';
        return false;
    }

    if (kill(-job->pgid, signalNumber) == -1) {
        perror("kill");
        return false;
    }

    return true;
}

int executeCommand(const Command& command) {
    if (command.program.empty()) {
        return 0;
    }

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
        restoreChildSignals();

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

        if (!command.outputFile.empty()) {
            int flags = O_WRONLY | O_CREAT |
                    (command.outputAppend ? O_APPEND : O_TRUNC);
            int outputFd = open(command.outputFile.c_str(), flags, 0644);
            if (outputFd == -1) {
                perror(command.outputFile.c_str());
                _exit(1);
            }
            if (dup2(outputFd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(outputFd);
                _exit(1);
            }
            if (outputFd != STDOUT_FILENO) {
                close(outputFd);
            }
        }

        execvp(argv[0], argv.data());
        int error = errno;
        perror(argv[0]);
        _exit(error == ENOENT ? 127 : 126);
    }

    int status = 0;
    pid_t result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
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

    installChildHandler();

    std::vector<std::array<int, 2>> pipes(commandCount > 0 ? commandCount - 1 : 0);
    for (auto& pipeFds : pipes) {
        pipeFds = {-1, -1};
    }

    for (auto& pipeFds : pipes) {
        if (pipe(pipeFds.data()) == -1) {
            perror("pipe");
            closePipes(pipes);
            return 1;
        }
    }

    std::vector<pid_t> pids;
    bool forkFailed = false;
    pid_t jobPgid = 0;
    const std::string commandText = describePipeline(pipeline);

    for (size_t i = 0; i < commandCount; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            forkFailed = true;
            break;
        }

        if (pid == 0) {
            pid_t childPgid = jobPgid == 0 ? getpid() : jobPgid;
            if (setpgid(0, childPgid) == -1) {
                perror("myshell: child setpgid");
                _exit(1);
            }

            restoreChildSignals();
            const Command& command = pipeline.commands[i];

            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }
            if (i + 1 < commandCount &&
                dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }

            for (const auto& pipeFds : pipes) {
                if (pipeFds[0] != -1) close(pipeFds[0]);
                if (pipeFds[1] != -1) close(pipeFds[1]);
            }

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
                int flags = O_WRONLY | O_CREAT |
                (command.outputAppend ? O_APPEND : O_TRUNC);
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
            if (!command.errorFile.empty()) {
                int flags = O_WRONLY | O_CREAT |
                (command.errorAppend ? O_APPEND : O_TRUNC);
                int fd = open(command.errorFile.c_str(), flags, 0644);
                if (fd == -1) {
                    perror(command.errorFile.c_str());
                    _exit(1);
                }
                if (dup2(fd, STDERR_FILENO) == -1) {
                    perror("dup2");
                    close(fd);
                    _exit(1);
                }
                if (fd != STDERR_FILENO) close(fd);
            }
            if (command.errorToStdout) {
                if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                }
            }

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
            int error = errno;
            perror(argv[0]);
            _exit(error == ENOENT ? 127 : 126);
        }

        pids.push_back(pid);
        if (jobPgid == 0) {
            jobPgid = pid;
        }

        if (setpgid(pid, jobPgid) == -1 &&
            errno != EACCES && errno != ESRCH && errno != EPERM) {
            perror("myshell: parent setpgid");
        }
    }

    closePipes(pipes);

    if (forkFailed) {
        if (jobPgid > 0) {
            kill(-jobPgid, SIGTERM);
        }
        for (pid_t childPid : pids) {
            int status = 0;
            while (waitpid(childPid, &status, 0) == -1) {
                if (errno == EINTR) continue;
                break;
            }
        }
        return 1;
    }

    if (pipeline.background) {
        if (!pids.empty()) {
            addJob(jobPgid, pids, {}, commandText, JobState::Running);
            std::cout << '[' << jobs.back().id << "] "
                      << pids.front() << '\n';
        }
        return 0;
    }

    bool terminalTransferred = false;
    if (isatty(STDIN_FILENO)) {
        if (tcsetpgrp(STDIN_FILENO, jobPgid) == -1) {
            perror("myshell: tcsetpgrp");
        } else {
            terminalTransferred = true;
        }
    }

    std::vector<pid_t> livePids = pids;
    std::vector<pid_t> stoppedPids;
    int lastStatus = 0;
    bool jobStopped = false;

    if (!waitForProcessGroup(
            jobPgid,
            livePids,
            stoppedPids,
            pids.empty() ? -1 : pids.back(),
            lastStatus,
            jobStopped)) {
        lastStatus = 1;
    }

    if (terminalTransferred &&
        tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("myshell: restore terminal");
    }

    if (jobStopped && !livePids.empty()) {
        addJob(
            jobPgid,
            livePids,
            stoppedPids,
            commandText,
            JobState::Stopped
        );
        std::cout << '[' << jobs.back().id << "] Stopped\t"
                  << jobs.back().command << '\n';
    }

    return lastStatus;
}
