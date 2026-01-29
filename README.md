# CFBASIC - Compact Factor BASIC

A Microsoft BASIC interpreter written in C for modern systems (Linux, macOS, Windows). Features a full-screen Commodore Kernal-style editor, C64 virtual RAM emulation, and hardware-mapped PEEK/POKE.

## Features

- **C128 BASIC 7.0 Esque Dialect**: Supports structured programming with `DO...LOOP`, `WHILE...WEND`, `REPEAT...UNTIL`, and `IF...THEN...ELSE`.
- **Case-Insensitive Syntax**: Commands and variables are not case-sensitive.
- **CBM Kernal-style Screen Editor**:
  - Full-screen cursor movement via arrow keys.
  - **Logical Line Picking**: Move the cursor to any line of text and press **ENTER** to execute it immediately.
  - Integrated virtual screen buffer for classic terminal interaction.
  - **Dual-Mode Driver**: Native **Win32 Console API** for Windows and **ANSI escape sequences** for Linux/macOS.
- **C64 Memory Compatibility**:
  - Emulated **64KB RAM** system.
  - Full **`PEEK`** and **`POKE`** support for all 65,536 addresses.
  - **Screen RAM Mapping**: Writing to `1024-2023` directly updates the terminal display.
  - **Hardware Traps**: VIC-II register emulation for colors (`53280/53281`).
- **Graphics Support**:
  - **`PLOT X, Y`** and **`DRAW X, Y`** for character-based line drawing.
  - Coordinates are scaled from standard C64 resolution (320x200) to your terminal window.
- **Memory Management**: Detailed RAM statistics including **Free**, **Used**, and **Allocated** memory.
- **Cross-Platform**: Native builds for Linux, macOS, and Windows.

## Building

### Linux/macOS

```bash
make
```

### Windows (MinGW)

```bash
make windows
```

## Usage

### Interactive Mode

```bash
./basic
```

### Run a Program/Script

```bash
./basic program.bas
```

### Control Keys

- **Ctrl+C**: Break a running program and return to the `READY.` prompt.
- **EXIT**: Type `EXIT` in immediate mode to quit the interpreter.

## BASIC Commands

### Immediate Commands

- `LIST` - Display program
- `RUN` - Execute program
- `NEW` - Clear program
- `LOAD "filename"` - Load program from file
- `SAVE "filename"` - Save program to file
- `EXIT` - Quit the interpreter
- `HELP` - Display help information
- `CLR` - Clear the console screen
- `MEMCHK` - Display detailed memory statistics

### Program Statements

- `PRINT` / `?` - Output text/values
- `INPUT` - Read user input
- `LET` - Variable assignment
- `GOTO` - Jump to line number
- `GOSUB` / `RETURN` - Subroutine calls
- `IF...THEN...ELSE` - Conditional execution
- `FOR...NEXT` - Loops
- `DO...LOOP` - Structured loop (C128)
- `WHILE...WEND` - While loop (C128)
- `REPEAT...UNTIL` - Repeat-until loop (C128)
- `POKE addr, val` - Write to emulated RAM
- `PLOT x, y` - Set drawing position
- `DRAW x, y` - Draw line to coordinate
- `REM` - Comments
- `END` / `STOP` - End program
- `DIM` - Declare arrays
- `CLR` - Clear the console screen
- `MEMCHK` - Display memory statistics

### Built-in Functions

- `PEEK(addr)` - Read from emulated RAM
- `ABS(x)` - Absolute value
- `INT(x)` - Integer part
- `RND(x)` - Random number
- `SIN(x)`, `COS(x)`, `TAN(x)` - Trigonometric functions
- `SQR(x)` - Square root
- `LEN(s$)` - String length
- `LEFT$(s$,n)`, `RIGHT$(s$,n)`, `MID$(s$,n,m)` - String functions
- `STR$(x)` - Number to string
- `VAL(s$)` - String to number
- `CHR$(x)` / `ASC(s$)` - Character/ASCII conversion

## Examples

### Number Speed Test (Benchmark)

```basic
10 FOR I = 1 TO 10000
20 PRINT ".";
30 NEXT I
40 PRINT "DONE."
```

### Classic Hello, World! Test

```basic
10 PRINT "Hello, World!"
20 GOTO 10
```

## Installation (Linux/macOS Only)

```bash
sudo make install
```

This installs `basic` to `/usr/local/bin`.

## License

MIT License - See [LICENSE](LICENSE) file for details.

## Author

This program is developed by @NovaDevThings on YouTube.
