# myshell

A minimal, interactive Unix-like shell built from scratch in C++17 as a learning project for operating system concepts — process management, file descriptors, and parsing.

```
myshell> ls -l
myshell> echo hello > out.txt
myshell> cat < test.txt
myshell> exit
```

## Overview

`myshell` implements the classic shell pipeline: read a line of input, tokenize it, parse it into a structured `Command`, then either run a builtin in-process or `fork()`/`execvp()` an external program — with support for input/output/append redirection along the way.

There are no external dependencies: just a C++17 compiler, `make`, and a POSIX system (Linux, macOS, or WSL).

## Features

| Feature | Notes |
|---|---|
| Interactive REPL | Prompt `myshell> `, exits on `exit` or EOF (Ctrl-D) |
| Tokenization | Whitespace-delimited lexing with `'...'` / `"..."` quote grouping |
| Command parsing | Tokens → `Command` struct, with parse-error reporting |
| Builtin commands | `cd` (with `$HOME` fallback), `pwd`, `exit` — run without forking |
| External commands | `fork()` + `execvp()` with `PATH` lookup, `waitpid()` in parent |
| Input redirection | `< file` |
| Output redirection | `> file` (truncate) |
| Append redirection | `>> file` |
| Exit-status conventions | `127` command not found, `126` exec failed, `128+N` killed by signal N |

### Not yet supported

- Pipes (`|`) and command sequences (`;`, `&&`, `||`)
- Escaping (`\`) and unbalanced-quote handling — single/double quotes group words, but there is no backslash escape
- Variable expansion (`$VAR`, `~`) and the `$?` status variable
- Signals / job control (Ctrl-C, Ctrl-Z, `&`, `fg`, `bg`, `jobs`) — **Ctrl-C currently terminates the shell itself**
- History, line editing (arrow keys), and tab completion
- Redirection applied to builtins (`cd > out.txt` does not redirect)
- Environment builtins (`export`, `unset`, `alias`, `echo`, `help`)
- stderr redirection (`2>`, `2>&1`)

## Requirements

- A POSIX operating system (Linux, macOS, or WSL) — the code uses raw POSIX headers, so it will not build natively on Windows
- `g++` (or another C++17-capable compiler — the Makefile sets `CXX = g++`)
- GNU `make`

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

Type `exit` or press Ctrl-D on an empty line to quit.

> **Note:** the Makefile compiles all sources in a single invocation with no header dependency tracking. After editing a header, run `make clean && make`.

## Usage examples

```text
myshell> pwd
myshell> cd /tmp
myshell> cd
myshell> ls -l
myshell> cat < test.txt
myshell> echo hello > out.txt
myshell> echo more >> out.txt
```

Redirection operators must be space-separated from other tokens (`echo hi > out.txt`, not `echo hi>out.txt`).

## How it works

```
stdin
  │
  ▼
main.cpp  ── REPL: read line, print prompt
  │
  ▼
tokenize()  ── split on whitespace (quotes group words)  →  vector<string>
  │
  ▼
parse()     ── tokens → Command{program, arguments,
  │                             inputFile, outputFile, append}
  ▼
isBuiltin? ── yes ──► executeBuiltin()   (cd/pwd/exit — no fork)
  │
  no
  ▼
executeCommand()
  ├─ fork()
  ├─ child:  open() → dup2() for < and > / >>  →  execvp()
  │          on failure: _exit(127) for ENOENT, else _exit(126)
  └─ parent: waitpid() (retrying on EINTR) → propagate exit status
```

Each stage lives in its own translation unit with a small public header, so the parser knows nothing about processes and the executor knows nothing about tokens. The `Command` struct acts as the intermediate representation between them.

## Project structure

```
myshell/
├── Makefile                     # build / run / clean targets
├── README.md
├── test.txt                     # sample file for redirection demos
└── src/
    ├── main.cpp                 # REPL & dispatcher
    ├── parser/
    │   ├── tokenizer.h/.cpp     # whitespace lexer
    │   ├── parser.h/.cpp        # tokens → Command, validates redirection
    │   └── command.h            # Command data model (the IR)
    ├── builtins/
    │   ├── builtins.h
    │   └── builtins.cpp         # cd, pwd, exit
    ├── executer/
    │   ├── executer.h
    │   └── executer.cpp         # fork + redirection + execvp + waitpid
    └── tests/
        └── tokenizer_test.cpp   # manual tokenizer smoke test
```

## Key learnings

This project started as a way to learn how a shell actually works under the hood. The most valuable takeaways:

1. **The `fork()` / `exec()` process model.** A shell doesn't "run" commands — it clones itself and replaces the clone's memory image with the target program. Redirection and `execvp()` must happen in the **child**, after the fork, otherwise the shell's own streams would be clobbered.

2. **File descriptors and `dup2()`.** Redirection is just: `open()` a file, `dup2(fd, STDIN_FILENO/STDOUT_FILENO)` in the child, then close the original fd — being careful to skip the close when `fd` already *is* the standard descriptor. Also learned the difference between `O_TRUNC` and `O_APPEND`, and file mode `0644`.

3. **`_exit()` vs `exit()` in a forked child.** Calling `exit()` in the child would flush and duplicate buffered stdio data belonging to the parent. The child must use `_exit()`.

4. **Why builtins can't be forked.** `cd` in a child process would change the child's directory and immediately discard it on exit; `exit` must terminate the shell itself. Only builtins that affect shell state run in-process — everything else forks.

5. **Exit-status conventions.** Learned to read statuses properly with `WIFEXITED`/`WEXITSTATUS` and `WIFSIGNALED`/`WTERMSIG`, and the shell conventions: **127** for command-not-found (`ENOENT`), **126** for a found-but-unrunnable executable, and **128+N** when killed by signal N.

6. **`waitpid()` can be interrupted.** Signals interrupt syscalls, so `waitpid` must be retried in a `while (... == -1 && errno == EINTR)` loop rather than treated as failure.

7. **Layered parsing with a simple IR.** Splitting lexing (tokenizer) from parsing (parser) from execution, with a `Command` struct in between, keeps each stage independently testable — and leaves a clean seam for future features like pipes (a `vector<Command>` + pipe fds).

8. **Error handling at the right boundary.** Parse errors are thrown as `std::runtime_error` and caught at the REPL level, so a bad line reports an error instead of killing the shell.

9. **Repo hygiene.** Early commits accidentally included compiled binaries (`src/main`, `src/myshell`); a cleanup commit removed them and added `.gitignore` rules — a concrete lesson in never committing build artifacts.

10. **Open-source workflow.** The executor and redirection features were developed on feature branches, reviewed via pull requests, and merged — the first hands-on experience with GitHub collaboration.

## Testing

There is currently no automated test suite or `make test` target. A small manual smoke test for the tokenizer exists at `src/tests/tokenizer_test.cpp`; run it with:

```bash
g++ -std=c++17 -Wall -Wextra -g -Isrc/parser \
    src/tests/tokenizer_test.cpp src/parser/tokenizer.cpp -o tokenizer_test
./tokenizer_test
```

Expected output:

```
[ls]
[-l]
[/home]
---
[echo]
[Hello world]
[single quoted]
```

## Roadmap

- [ ] Pipes (`ls | grep foo`)
- [x] Quotes (`'...'`, `"..."`); escapes (`\`) still pending
- [ ] `;`, `&&`, `||` sequencing
- [ ] `$VAR` and `~` expansion; `$?` status variable
- [ ] Signal handling (Ctrl-C ignored in shell, delivered to foreground child)
- [ ] Job control (`&`, `jobs`, `fg`, `bg`)
- [ ] Command history and tab completion (e.g. via GNU readline)
- [ ] Redirection for builtins and stderr redirection (`2>`)
- [ ] A `make test` target and real test suite
