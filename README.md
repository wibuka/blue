## Prerequisites

### Linux
#### Compiler
- GNU Compiler Collection (GCC) 8.1 or later, **or**
- Clang 15 or later
#### Libraries & Tools
- **LLVM 22** — Required for code analysis features.
  - Ubuntu/Debian: `sudo apt install llvm-22`
  - Arch Linux: Install LLVM 22 using your package manager.
- **Clang** — Required for compiling and linking generated code.
  - Ubuntu/Debian: `sudo apt install clang`
  - Arch Linux: `sudo pacman -S clang`
  - Clang must be installed and available in the system `PATH`.

### Windows
#### Compiler
- MinGW-w64 (GCC) for MinGW builds.
#### Libraries & Tools
- **LLVM 22** — Required for code analysis features.
  - Download from: https://github.com/llvm/llvm-project/releases

- **Clang** — Required for compiling and linking generated code.
  - Clang must be installed and added to the system `PATH` environment variable.

### macOS
- Not supported yet.

## Build Instructions
```bash
# Linux
mkdir build
cd build
cmake ..
cmake --build .

# Windows (MinGW)
cd C:\path\to\project
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Dependencies
The project depends on:
- **CMake 3.16 or later**
- **Zlib** (vendored library)
- **Lua 5.4** (vendored library)
- **LuCI** (vendored library)
- **miniaudio** (vendored library)

All dependencies are vendored and included in the project, so no external
installation is required.
## Usage

### Getting started
```blue
import "core/libc"

def main() {
    printf("I love you <3\n")
}
```
### Compiling a program
```bash
blue path/to/program.blue -o program
```
### Emitting intermediate forms
```bash
blue program.blue -s -o program.s     # assembly
blue program.blue -c -o program.o     # object file
blue program.blue -ll -o program.ll   # LLVM IR
```
### Optimization levels
```bash
blue -O2 program.blue -o program
```
