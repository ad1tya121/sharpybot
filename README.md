# sharpybot

A personalized chess assistant built on a custom UCI chess engine and reinforcement learning. For players who want more than just the best move.

## Requirements

- [Git](https://git-scm.com/downloads)
- [CMake](https://cmake.org/download/) 3.10 or newer
- A C++20 compiler:
  - **Linux/macOS**: GCC or Clang (usually pre-installed, or via `xcode-select --install` on macOS / your package manager on Linux)
  - **Windows**: [MSYS2/MinGW-w64](https://www.msys2.org/) or Visual Studio Build Tools with C++ support

## HOW TO USE

Run the script corresponding to your OS. It will automatically clone the repository, build the binary, and print the exact file path to the executable at the end.

### Linux / macOS

```bash
bash -c "$(curl -fsSL https://raw.githubusercontent.com/ad1tya121/sharpybot/main/run-sharpybot.sh)"
```

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/ad1tya121/sharpybot/main/run-sharpybot.ps1 | iex
```
