# System Architecture & Technical Design Document (MyTerm)

## 1. Executive Summary & Philosophy
- **System Overview**: MyTerm combines a POSIX Linux command shell interpreter with an event-driven graphical terminal emulator engineered directly on raw X11 (Xlib) protocols.
- **Design Rationale**: Bypassing heavyweight GUI frameworks (like GTK or Qt) in favor of native C Xlib primitives provides complete control over memory character buffers, font glyph rendering, and window event queues.
- **Architectural Goal**: Enforce rigorous separation of concerns between low-level operating system mechanisms (process virtualization, piping, descriptor polling) and visual user interface loops to ensure testability and extensibility.

---

## 2. Modular Architecture & GUI Decoupling

### Separation of Concerns (MVC Design Pattern)
- **Problem Solved**: Replaced an early academic 1,900-line monolithic source file (`myterm.c`) where debugging parser edge cases required manually launching a graphical X11 window.
- **Data Definitions (`include/common.h`)**: Centralizes shared state structures (`Terminal`, `Tab`, `BackgroundJob`) into clean header interfaces without exposing graphical libraries to backend modules.
- **Domain Execution Engine (`src/exec.c`, `src/history.c`, `src/io.c`)**: Functionally isolated system libraries responsible for command tokenization, string searching algorithms, pipeline routing, and process signal trapping.
- **Presentation Layer (`src/ui.c`, `src/main.c`)**: Strictly contains Xlib window handles, graphics contexts (`GC`), input method editors (`XIM`/`XIC`), and event drawing loops (`Expose`, `KeyPress`, `SelectionNotify`).

### Headless Unit Testing via Conditional Compilation
- **Preprocessor Decoupling**: Wraps functions interacting with Xlib inside preprocessor compilation guards:
  ```c
  #ifndef WITHOUT_X11
  void draw_terminal(Terminal *term);
  #endif
  ```
- **CI/CD Compatibility**: Passing `-DWITHOUT_X11` to GCC during test compilation (`make test`) strips away GUI rendering routines while preserving string parsers and Dynamic Programming algorithms for automated headless verification.

---

## 3. Core Data Structures & State Management

### Global Terminal State (`Terminal`)
- Manages global X Server window properties, color schemes, font metrics, and dynamic tab arrays:
  ```c
  typedef struct {
      Display *display;        // Pointer to the X Server display connection
      int screen;              // Default screen identifier
      Window window;           // Main terminal X11 window handle
      GC gc;                   // Graphics context used for drawing text, shapes, and cursors
      XFontStruct *font;       // Monospaced loaded font metrics
      XIM xim;                 // X11 Input Method (for international/locale support)
      XIC xic;                 // X11 Input Context
      int window_width;        // Current dynamic width of the window
      int window_height;       // Current dynamic height of the window
      unsigned long grey_color;// Allocated color palette pixel
      Tab **tabs;              // Dynamic array of pointer references to individual tab sessions
      int num_tabs;            // Total active tabs currently running
      int current_tab;         // Index of the currently selected/visible tab
      int tab_capacity;        // Current heap allocation capacity for tab pointers
  } Terminal;
  ```

### Isolated Tab Session Context (`Tab`)
- Isolates text logs and execution environments per tab to ensure zero output crossover during switching:
  ```c
  typedef struct {
      char buffer[MAX_BUFFER];      // 20,000-character ring-style scrollable output history
      int buffer_len;               // Active length of text currently in buffer
      int scroll_offset;            // Vertical scroll position when inspecting old logs
      int user_scrolling;           // Flag indicating if the user has locked manual scroll view
      char input[1024];             // Current command prompt line being typed
      int input_len;                // Number of characters in the input buffer
      int cursor_pos;               // Current horizontal index of the flashing text cursor
      int continuation_mode;        // State flag when handling multi-line inputs with backslashes (\)
      int search_mode;              // Flag indicating if interactive Ctrl+R history search is open
      int search_input_len;         // Length of active Ctrl+R query string
      char search_input[256];       // Buffer storing the active search filter
      char **ac_matches;            // Dynamic array holding candidate file matches during autocompletion
      int ac_count;                 // Total matching items discovered during Tab press
      int ac_waiting_choice;        // State indicating the shell is expecting a numerical menu selection
  } Tab;
  ```
- **Automatic Buffer Trimming**: Triggers a clean memory trim algorithm (`trim_buffer`) whenever text reaches 80% of `MAX_BUFFER`, discarding the oldest top 20% of text lines cleanly at newline boundaries to prevent RAM exhaustion.

---

## 4. Deep Dive: Algorithmic Engineering

### Longest Common Substring (LCS) for History Search (`Ctrl+R`)
- **Algorithmic Objective**: Replaces classic static prefix matching with an algorithmic fallback that finds past commands sharing the largest consecutive sequence of matching characters when typos occur.
- **Space Complexity Optimization**: Optimizes classical $O(N \cdot M)$ Dynamic Programming 2D heap matrices down to an optimal **$O(M)$ auxiliary space structure** using two rotating linear 1D arrays (`prev` and `cur`).
- **Time Complexity**: Runs in an efficient $O(N \cdot M)$ time window across string comparison lengths.
- **Chronological Priority Resolution**: Evaluates stored history loops backward from newest (`history_count - 1`) down to oldest (`0`) using a strict greater-than inequality (`if (lcs > max_lcs)`). This ensures that when two history strings share identical overlap lengths, the engine naturally prioritizes the command executed most recently.

### Intelligent Autocompletion via Longest Common Prefix (LCP)
- **Directory Scanning**: When `Tab` is pressed, the shell opens the active workspace using `opendir()` / `readdir()` to gather all filenames matching the typed prefix token under the cursor.
- **Automatic Prefix Extension**: Passes multiple file candidates into an iterative **Longest Common Prefix (LCP)** algorithm that verifies character column equivalence and auto-appends any shared unambiguous letters directly into the input line.
- **Interactive Menu Routing**: When file candidates diverge, the engine formats a numbered menu directly into the GUI screen (`1. file_a.c`, `2. file_b.c`), shifting tab state (`ac_waiting_choice = 1`) to intercept the subsequent number keystroke and insert the chosen file path in $O(1)$ time.

---

## 5. OS System Calls & IPC Mechanics

### Unix Pipelines & File Redirection
- **I/O Redirection**: Scans raw command tokens for `<` and `>` operators in `parse_redirection()`. Before executing system programs, child processes open target descriptors using `open()` and overwrite standard OS handles (`STDIN_FILENO`, `STDOUT_FILENO`) via `dup2()`.
- **Multi-Stage Piping**: Chained workflows (`cmd1 | cmd2 | cmd3`) tokenize strings by `|` delimiters and instantiate an array of POSIX pipes (`pipe()`). Each pipeline stage runs inside an isolated `fork()`, wiring write endpoints of pipe $i$ directly to read endpoints of pipe $i+1$.
- **Asynchronous GUI Polling**: Sets the final pipeline read descriptor to non-blocking mode (`O_NONBLOCK`) and multiplexes it via POSIX `poll()`. The main event loop reads incoming pipe bytes into text buffers while continuously checking X11 events (`XPending()`), preventing window UI freezing during intensive command pipeline outputs.

### Process Groups & Background Job Control
- **Process Group Isolation**: Executes `setpgid(0, 0)` immediately after forking child processes and before invoking `execvp()`. Assigning tools to isolated process groups prevents keyboard interrupts (`Ctrl+C` / `SIGINT`) from shutting down the parent terminal GUI window.
- **Job Registry (`Ctrl+Z`)**: Traps suspended foreground processes (`SIGTSTP`), transmits POSIX `SIGSTOP` signals to halt CPU thread execution, and stores process IDs inside a global `background_jobs[]` registry with clean numerical identifiers (`[1]+ Stopped`).
- **Process Resumption**: Resumes suspended jobs into active foreground (`fg <id>`) or continuous background (`bg <id>`) execution by transmitting POSIX `SIGCONT` signals directly to targeted process groups.

### Concurrent Live Monitoring (`multiWatch`)
- **Parallel Worker Forking**: Processes command arguments (e.g., `multiWatch ["date", "ls -l"]`) and forks $K$ background worker child processes.
- **Atomic Temp Logging**: Worker $i$ executes its system command every 2 seconds via `sleep()` and atomic redirect replacement into unique temporary files (`.temp.<pid>_i.txt`).
- **Multiplexed Polling**: The main terminal parent leverages non-blocking descriptors and POSIX `select()` multiplexing over temporary files to detect modification timestamp changes or file size updates, formatting real-time timestamped output headers directly into the UI buffer.

---

## 6. Automated Unit Testing Harness

- **Zero-Dependency Architecture**: Engineered a custom lightweight testing macro harness in `tests/test_harness.h` utilizing preprocessor assertions (`TEST_ASSERT`, `__FILE__`, `__LINE__`) to ensure instantaneous compilation without third-party testing library overhead.
- **LCS & History Verification Suite (`tests/test_lcs.c`)**: Validates Dynamic Programming string calculations, exact match precedence, fallback accuracy, empty query resilience, and reverse chronological priority ordering.
- **Command Parser Suite (`tests/test_parser.c`)**: Validates output/input redirection stripping, space preservation within complex quotation strings (`"my folder/file.c"`), automatic argument flag injections (`echo` -> `echo -e`), and multi-file LCP autocompletion math.
- **Validation Outcome**: Verified 100% test passing rates across 33 automated test assertions, proving that modular refactoring preserved complete functional integrity while upgrading academic code to production engineering standards.
