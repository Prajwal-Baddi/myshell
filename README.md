# myshell

A minimal, interactive Unix-like shell built from scratch in C++17 as a learning project for operating system concepts — process management, file descriptors, and parsing.

```
myshell> ls -l
myshell> echo hello > out.txt
myshell> cat < test.txt
myshell> ls | grep .cpp
myshell> export NAME=Hrishi
myshell> echo $NAME
myshell> sleep 10 &
myshell> jobs
myshell> exit
```

## Overview

`myshell` implements the classic shell pipeline: read a line of input, tokenize it, parse it into a structured `CommandSequence` containing `Pipeline`s, then either run a builtin in-process or `fork()` children to execute external programs—wiring them together with `pipe()`/`dup2()` and supporting complex redirection.

It uses GNU Readline for an interactive experience with command history and line editing.

## Features

| Feature | Notes |
|---|---|
| Interactive REPL | GNU Readline integration for history and editing; exits on `exit` or EOF (Ctrl-D) |
| Tokenization | Whitespace-delimited lexing with `'...'` / `"..."` quote grouping and `\` escape support |
| Pipelines | `cmd1 | cmd2 | ...` — one `pipe()` per gap, one child per stage; exit status of the last stage |
| Command Parsing | Support for `;`, `&&`, `||` sequencing and `&` background execution |
| Builtin commands | `cd`, `pwd`, `exit`, `jobs`, `fg`, `bg`, `kill`, `export`, `unset`, `echo`, `help` |
| External commands | `fork()` + `execvp()` with `PATH` lookup, `waitpid()` in parent |
| Redirection | Input (`<`), Output (`>`, `>>`), and stderr (`2>`, `2>&1`) |
| Environment | Expansion of `$VAR`, `${VAR}`, `$?`, and `~` (home directory) |
| Job Control | Process group management, `fg`/`bg` transitions, and background child reaping |
| Exit-status conventions | `127` command not found, `126` exec failed, `128+N` killed by signal N |

## Requirements

- A POSIX operating system (Linux, macOS, or WSL)
- `g++` (C++17 capable)
- GNU `make`
- `libreadline-dev` (for interactive history and editing)

## Getting started

```bash
git clone https://github.com/Prajwal-Baddi/myshell.git
cd myshell
make          # builds ./myshell
./myshell     # start the shell
```

Or simply:

```bash
make run      # build if needed, then launch
make clean    # remove the built binary
```

## Usage examples

```text
myshell> pwd
myshell> cd /tmp
myshell> ls -l
myshell> cat < test.txt
myshell> echo hello > out.txt
myshell> echo more >> out.txt
myshell> ls | grep .cpp | wc -l
myshell> export NAME=Hrishi
myshell> echo "Hello $NAME"
myshell> sleep 10 &
myshell> jobs
myshell> fg %1
myshell> true && echo "Success"
myshell> false || echo "Fallback"
```

## How it works

```
stdin (via Readline)
  │
  ▼
main.cpp  ── REPL: read line, print prompt
  │
  ▼
tokenize()  ── split on whitespace (quotes group words, escapes handled)  →  vector<string>
  │
  ▼
expand()    ── expand $VAR, ${VAR}, $?, and ~  →  vector<string>
  │
  ▼
parse()     ── tokens → CommandSequence { pipelines: [ Pipeline { commands: [...] } ], operators: [...] }
  │
  ▼
executor    ── executeSequence()
              ├─ check operators (&&, ||, ;)
              ├─ executePipeline()
              │    ├─ create pipes, fork children, set pgid
              │    ├─ child: dup2() pipes/files → execvp()
              │    └─ parent: waitpid() group or add to jobs list
              └─ executeBuiltin() (run in-process)
```

## Project structure

```
myshell/
├── Makefile                     # build / run / clean targets
├── README.md
└── src/
    ├── main.cpp                 # REPL & dispatcher
    ├── parser/
        ├── tokenizer.h/.cpp     # lexer with quote/escape support
        ├── parser.h/.cpp        # tokens → CommandSequence/Pipeline
        └── command.h            # Data models (Command, Pipeline, CommandSequence)
    ├── builtins/
        ├── builtins.h
        └── builtins.cpp         # all shell built-in implementations
    ├── executer/
        ├── executer.h
        └── executer.cpp         # process management, piping, and job control
    └── tests/
```

## Key learnings

1. **The `fork()` / `exec()` process model.** Learned that redirection and signal restoration must happen in the child process to avoid clobbering the shell.
2. **File descriptors and `dup2()`.** Implemented complex redirection including stderr to stdout (`2>&1`).
3. **Job Control & Process Groups.** Used `setpgid` and `tcsetpgrp` to manage terminal ownership and deliver signals to specific pipelines.
4. **GNU Readline Integration.** Learned to integrate external C libraries for professional CLI interactions.
5. **Layered Parsing.** Moving from simple string splitting to a structured `CommandSequence` IR allowed for implementing `&&` and `||` naturally.

## Testing

Integration tests are available in `src/tests/integration/test_shell.sh`. Run them with:

```bash
./src/tests/integration/test_shell.sh
```
