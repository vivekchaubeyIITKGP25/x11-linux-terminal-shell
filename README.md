# MyTerm — Custom X11 Terminal Emulator & Linux Shell

## Project Overview
- **Core Engine**: A custom terminal emulator and Linux command-line shell engineered entirely from scratch in **pure C** using raw **X11 (Xlib) graphics protocols**.
- **System Bridging**: Directly bridges low-level POSIX operating system concepts (process virtualization, inter-process communication, non-blocking descriptor multiplexing) with event-driven GUI software design.
- **Production Architecture**: Refactored from a course project monolithic codebase into a cleanly decoupled, test-driven modular engineering system.

---

## Key Technical Accomplishments
- **MVC-Style Modularity**: Split a 1,900-line monolithic codebase into clean interface headers (`include/`) and independent functional domain modules (`src/`).
- **Headless UI Decoupling**: Separated X11 graphical rendering from system shell mechanics via conditional preprocessor guards (`-DWITHOUT_X11`), allowing algorithms and command parsers to be unit tested without launching a graphical window or X server.
- **Automated Verification**: Engineered a custom zero-dependency testing macro harness in C and created automated test build targets (`make test`) that verify 100% of algorithmic edge cases across 33 test assertions.

---

## Core Feature Highlights

### 1. Operating Systems Mechanics & IPC
- **Interactive Command Shell**: Displays a custom `user@myterm$` prompt with an active readline memory buffer.
- **Process Execution**: Runs system applications via POSIX `fork()` and `execvp()`, featuring environment wildcard globbing (`*`, `?`).
- **Multi-Stage Unix Pipelines**: Chains continuous execution streams (`cmd1 | cmd2 | cmd3`) using asynchronous POSIX pipes (`pipe()`) and descriptor cloning (`dup2()`).
- **File I/O Redirection**: Handles standard input/output redirection (`<` and `>`) before process invocation.

### 2. Process Groups & Signal Control
- **Background Job Table**: Full background process tracking supporting job interception (`Ctrl+Z`), listing (`jobs`), and foreground/background resumption (`fg <id>`, `bg <id>`).
- **Process Group Isolation**: Executes `setpgid()` on child processes to assign distinct process groups, guaranteeing that terminal keyboard interrupts (`Ctrl+C` / `SIGINT`) cleanly stop running programs without terminating the root GUI window.

### 3. Algorithmic Intelligence
- **Dynamic Programming History Search**: An interactive search engine (`Ctrl+R`) that checks for exact string matches and falls back to an **$O(M)$ rolling-row Longest Common Substring (LCS)** algorithm when typos or partial terms occur.
- **Intelligent Autocompletion**: A filename completion system (`Tab`) that evaluates directory files using **Longest Common Prefix (LCP)** math to automatically extend shared prefixes or format an interactive numbered selection menu.

### 4. GUI & User Experience (Xlib)
- **Multi-Tabbed Interface**: Dynamically create new independent terminal instances (`Ctrl+T`), close tabs, or switch between active tabs instantly via keyboard shortcuts (`Alt+1..9`).
- **Scrollable Output Buffers**: Inspect historical logs across a 20,000-character ring memory buffer using the mouse wheel or keyboard arrows (`Ctrl+Up/Down`).
- **System Clipboard Integration**: Copy text or paste commands directly to/from the operating system clipboard (`Ctrl+Shift+C/V`) using X11 `UTF8_STRING` selection atoms.
- **Concurrent Polling Monitor**: Includes `multiWatch`, an advanced utility that executes and displays timestamped output from multiple parallel background processes every 2 seconds via POSIX `select()` multiplexing.

---

## Project Structure & Layout

```
myterm/
├── Makefile                # Build targets for the GUI application and automated unit test suite
├── README.md               # Quickstart overview (this file)
├── DESIGNDOC.md            # Deep-dive architectural design and system algorithms document
├── include/                # Module interface headers
│   ├── common.h            # Core structures (Terminal, Tab, BackgroundJob) and shared system constants
│   ├── history.h           # Command history loading, saving, and DP search prototypes
│   ├── io.h                # POSIX signal traps and scroll buffer memory trimming routines
│   ├── exec.h              # Command tokenization, quotation parsing, pipelines, and multiWatch logic
│   └── ui.h                # X11 drawing routines, font metrics, and keyboard/mouse event loop handlers
├── src/                    # C implementation modules
│   ├── history.c           # History file persistence (~/.myterm_history) & dynamic programming LCS algorithm
│   ├── io.c                # Signal traps, background process registry, and output buffer management
│   ├── exec.c              # Shell tokenization, redirection parsing, pipelines, and multiWatch polling
│   ├── ui.c                # Xlib graphics rendering, tab painting, and event loops
│   └── main.c              # Application startup, XIM/XIC locale initialization, and shutdown cleanups
└── tests/                  # Headless automated unit testing harness
    ├── test_harness.h      # Custom zero-dependency testing macro framework
    ├── test_lcs.c          # Unit tests for longest common substring calculation & history fallback
    └── test_parser.c       # Unit tests for command tokenization, quotes, redirection, and LCP autocomplete
```

---

## Getting Started & Building

### 1. Prerequisites
Ensure standard C compilation tools and X11 graphics headers are installed:
```bash
# On Debian / Ubuntu Linux:
sudo apt update && sudo apt install build-essential libx11-dev
```

### 2. Compiling & Running MyTerm
- Build the main executable by running:
  ```bash
  make
  ```
- Launch the X11 terminal emulator directly in an X-window desktop session:
  ```bash
  ./myterm
  ```

---

## Automated Unit Testing

- **Headless Execution**: Because Xlib graphical functions are isolated behind preprocessor flags (`-DWITHOUT_X11`), automated unit tests can compile and run directly from standard bash terminal environments, WSL, or CI/CD servers without an active GUI window.
- **Running Tests**: Build and run the verification suite via:
  ```bash
  make test
  ```

**Sample Verified Test Log:**
```
==========================================
       MyTerm LCS - Unit Test Suite       
==========================================

--- Running LCS Length Tests ---
  [PASS] Identical strings match full length
  [PASS] Partial overlap ('term') has length 4
  [PASS] Substring match ('myterm') has length 6
  [PASS] Disjoint strings return 0
  [PASS] Empty string returns 0

--- Running History Search & LCS Fallback Tests ---
  [PASS] Exact search should find result
  [PASS] Exact search sets is_exact flag to 1
  [PASS] Exact match returns correct command
  [PASS] LCS search should find result for partial query
  [PASS] LCS fallback sets is_exact flag to 0
  [PASS] LCS fallback correctly prefers the most recent matching command in history
  [PASS] LCS fallback finds specific earlier command when query uniquely matches
  [PASS] Short/non-matching search term (LCS <= 2) returns NULL

=== LCS & History Search Test Summary ===
Total: 13 | Passed: 13 | Failed: 0

==========================================
    MyTerm Parser - Unit Test Suite       
==========================================

--- Running Redirection Parsing Tests ---
  [PASS] Command truncated at output redirection symbol
  [PASS] Output file parsed correctly
  [PASS] Input file remains NULL when not redirected
  [PASS] Output redirection parsed in combined command
  [PASS] Input redirection parsed in combined command

--- Running Argument Tokenization Tests ---
  [PASS] Correct number of arguments parsed (handling quotes)
  [PASS] Arg 0 correct
  [PASS] Arg 1 correct
  [PASS] Arg 2 keeps spaces inside quotes
  [PASS] Arg 3 correct
  [PASS] Arg 4 correct
  [PASS] Null termination at argc
  [PASS] echo automatically injects -e argument
  [PASS] echo Arg 0 correct
  [PASS] echo Arg 1 is -e
  [PASS] echo Arg 2 correct
  [PASS] echo Arg 3 correct

--- Running Autocomplete Longest Common Prefix Tests ---
  [PASS] LCP across multiple files matching 'Make'
  [PASS] LCP matching 'test_' prefix
  [PASS] Disjoint string set returns empty prefix

=== Command Parser & Autocomplete Test Summary ===
Total: 20 | Passed: 20 | Failed: 0
All tests completed successfully!
```

---

## Quick Shortcuts Cheat Sheet

- **`Ctrl + T`**: Create and focus a new terminal tab
- **`Ctrl + W`** *(or click **X**)*: Close active terminal tab
- **`Alt + 1..9`**: Switch instantly between active tabs
- **`Ctrl + R`**: Open interactive command history search (matches exact queries or LCS fallbacks)
- **`Tab`**: Trigger LCP filename autocompletion or open interactive choice selection menus
- **`Ctrl + Shift + C` / `Ctrl + Shift + V`**: Copy or paste directly to/from OS system clipboard
- **`Ctrl + Z`**: Suspend running program to background job registry (`jobs`, `fg <id>`, `bg <id>`)
- **`Ctrl + C`**: Send `SIGINT` interrupt to abort foreground program or multiWatch monitor
- **`Ctrl + Q`**: Save session command history to disk and exit gracefully
- **`Ctrl + Up/Down`** *(or Mouse Wheel)*: Scroll terminal buffer history up or down
- **`multiWatch ["date", "ls -la"]`**: Run parallel polling workers to monitor concurrent Unix outputs every 2s

---
*For a deeper architectural breakdown of system call IPC mechanics, data structures, and algorithm complexity evaluations, reference [DESIGNDOC.md](./DESIGNDOC.md).*
