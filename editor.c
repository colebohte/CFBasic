#include "editor.h"
#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifndef _WIN32
static struct termios orig_termios;
#else
static DWORD orig_mode;
static HANDLE hStdout;
static HANDLE hStdin;
#endif

// Platform-abstracted terminal controls
static void term_clear(void) {
#ifdef _WIN32
  COORD coord = {0, 0};
  DWORD count;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(hStdout, &csbi);
  FillConsoleOutputCharacter(hStdout, (TCHAR)' ', csbi.dwSize.X * csbi.dwSize.Y,
                             coord, &count);
  SetConsoleCursorPosition(hStdout, coord);
#else
  printf("\x1b[2J\x1b[H");
#endif
}

static void term_move_cursor(int row, int col) {
#ifdef _WIN32
  COORD coord = {(SHORT)col, (SHORT)row};
  SetConsoleCursorPosition(hStdout, coord);
#else
  printf("\x1b[%d;%dH", row + 1, col + 1);
#endif
}

static void term_scroll_up(void) {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(hStdout, &csbi);
  SMALL_RECT scrollRect = {0, 1, (SHORT)(csbi.dwSize.X - 1),
                           (SHORT)(csbi.dwSize.Y - 1)};
  COORD dest = {0, 0};
  CHAR_INFO fill;
  fill.Char.AsciiChar = ' ';
  fill.Attributes = csbi.wAttributes;
  ScrollConsoleScreenBuffer(hStdout, &scrollRect, NULL, dest, &fill);
#else
  printf("\x1b[S");
#endif
}

void editor_enable_raw_mode(void) {
#ifdef _WIN32
  hStdin = GetStdHandle(STD_INPUT_HANDLE);
  hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE)
    return;

  GetConsoleMode(hStdin, &orig_mode);
  DWORD raw = orig_mode &
              ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
  SetConsoleMode(hStdin, raw);
#else
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    return;

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

void editor_disable_raw_mode(void) {
#ifdef _WIN32
  SetConsoleMode(hStdin, orig_mode);
#else
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
#endif
}

static void get_window_size(int *rows, int *cols) {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    *rows = 24;
    *cols = 80;
  } else {
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  }
#else
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    *rows = 24;
    *cols = 80;
  } else {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
  }
#endif
}

void editor_init(Editor *ed) {
  get_window_size(&ed->rows, &ed->cols);
  ed->cursor_row = 0;
  ed->cursor_col = 0;
  ed->buffer = safe_malloc(ed->rows * ed->cols);
  memset(ed->buffer, ' ', ed->rows * ed->cols);
}

void editor_free(Editor *ed) {
  if (ed->buffer) {
    safe_free(ed->buffer);
    ed->buffer = NULL;
  }
}

void editor_clear_screen(Editor *ed) {
  memset(ed->buffer, ' ', ed->rows * ed->cols);
  ed->cursor_row = 0;
  ed->cursor_col = 0;
  term_clear();
  fflush(stdout);
}

void editor_scroll(Editor *ed) {
  memmove(ed->buffer, ed->buffer + ed->cols, (ed->rows - 1) * ed->cols);
  memset(ed->buffer + (ed->rows - 1) * ed->cols, ' ', ed->cols);
  ed->cursor_row--;
  if (ed->cursor_row < 0)
    ed->cursor_row = 0;

  term_scroll_up();
}

void editor_refresh(Editor *ed) {
  // Basic refresh: clear screen and redraw from buffer
  term_move_cursor(0, 0);
  for (int r = 0; r < ed->rows; r++) {
    fwrite(ed->buffer + r * ed->cols, 1, ed->cols, stdout);
    if (r < ed->rows - 1)
      printf("\r\n");
  }
  term_move_cursor(ed->cursor_row, ed->cursor_col);
  fflush(stdout);
}

void editor_print(Editor *ed, const char *str) {
  while (*str) {
    if (*str == '\n') {
      ed->cursor_col = 0;
      ed->cursor_row++;
    } else if (*str == '\r') {
      ed->cursor_col = 0;
    } else if (*str == '\t') {
      ed->cursor_col = (ed->cursor_col + 8) & ~7;
    } else {
      if (ed->cursor_row >= ed->rows) {
        editor_scroll(ed);
      }
      ed->buffer[ed->cursor_row * ed->cols + ed->cursor_col] = *str;
      term_move_cursor(ed->cursor_row, ed->cursor_col);
      putchar(*str);
      ed->cursor_col++;
    }

    if (ed->cursor_col >= ed->cols) {
      ed->cursor_col = 0;
      ed->cursor_row++;
    }
    if (ed->cursor_row >= ed->rows) {
      editor_scroll(ed);
    }
    str++;
  }
  term_move_cursor(ed->cursor_row, ed->cursor_col);
  fflush(stdout);
}

static int get_char(void) {
#ifdef _WIN32
  return _getch();
#else
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1) {
    if (errno == EINTR)
      return -1;
    return -1;
  }
  return (unsigned char)c;
#endif
}

char *editor_read_line(Editor *ed) {
  while (1) {
    int char_val = get_char();
    if (char_val == -1)
      return NULL;
    char c = (char)char_val;

    if (c == '\r' || c == '\n') {
      // Pick the current line from logical screen
      int r = ed->cursor_row;
      int start = r * ed->cols;
      int end = start + ed->cols - 1;

      // Trim leading/trailing spaces for the "picked" line
      while (start <= end && ed->buffer[start] == ' ')
        start++;
      while (end >= start && ed->buffer[end] == ' ')
        end--;

      int len = end - start + 1;
      char *line = NULL;
      if (len > 0) {
        line = safe_malloc(len + 1);
        memcpy(line, ed->buffer + start, len);
        line[len] = '\0';
      } else {
        line = str_duplicate("");
      }

      // Move cursor to next line
      ed->cursor_row++;
      ed->cursor_col = 0;
      if (ed->cursor_row >= ed->rows) {
        editor_scroll(ed);
      }
      term_move_cursor(ed->cursor_row, ed->cursor_col);
      fflush(stdout);
      return line;
    } else if (c == 127 || c == 8) { // Backspace
      if (ed->cursor_col > 0) {
        ed->cursor_col--;
        ed->buffer[ed->cursor_row * ed->cols + ed->cursor_col] = ' ';
        printf("\b \b");
      }
    } else if (char_val == 224 || char_val == 0) { // Windows special keys
#ifdef _WIN32
      int next = _getch();
      switch (next) {
      case 72: // Up
        if (ed->cursor_row > 0)
          ed->cursor_row--;
        break;
      case 80: // Down
        if (ed->cursor_row < ed->rows - 1)
          ed->cursor_row++;
        break;
      case 77: // Right
        if (ed->cursor_col < ed->cols - 1)
          ed->cursor_col++;
        break;
      case 75: // Left
        if (ed->cursor_col > 0)
          ed->cursor_col--;
        break;
      }
      term_move_cursor(ed->cursor_row, ed->cursor_col);
#endif
    } else if (c == '\033') { // Escape sequence (POSIX)
#ifndef _WIN32
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1)
        continue;
      if (read(STDIN_FILENO, &seq[1], 1) != 1)
        continue;

      if (seq[0] == '[') {
        switch (seq[1]) {
        case 'A': // Up
          if (ed->cursor_row > 0)
            ed->cursor_row--;
          break;
        case 'B': // Down
          if (ed->cursor_row < ed->rows - 1)
            ed->cursor_row++;
          break;
        case 'C': // Right
          if (ed->cursor_col < ed->cols - 1)
            ed->cursor_col++;
          break;
        case 'D': // Left
          if (ed->cursor_col > 0)
            ed->cursor_col--;
          break;
        }
      }
      term_move_cursor(ed->cursor_row, ed->cursor_col);
#endif
    } else if (iscntrl(c)) {
      // Ignore other control codes
    } else {
      if (ed->cursor_row >= ed->rows) {
        editor_scroll(ed);
      }
      ed->buffer[ed->cursor_row * ed->cols + ed->cursor_col] = c;
      putchar(c);
      ed->cursor_col++;
      if (ed->cursor_col >= ed->cols) {
        ed->cursor_col = 0;
        ed->cursor_row++;
      }
    }
    fflush(stdout);
  }
  return NULL;
}

void editor_plot(Editor *ed, int x, int y, char c) {
  if (x < 0 || x >= ed->cols || y < 0 || y >= ed->rows)
    return;
  ed->buffer[y * ed->cols + x] = c;
  term_move_cursor(y, x);
  putchar(c);
  fflush(stdout);
}

void editor_set_background_color(Editor *ed, int color) {
  (void)ed;
#ifdef _WIN32
  // Map C64 colors (0-15) to Windows Console Attributes
  WORD attr = 0;
  switch (color & 15) {
  case 0:
    attr = 0;
    break; // Black
  case 1:
    attr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE |
           BACKGROUND_INTENSITY;
    break; // White
  case 2:
    attr = BACKGROUND_RED;
    break; // Red
  case 3:
    attr = BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
    break; // Cyan
  case 4:
    attr = BACKGROUND_RED | BACKGROUND_BLUE;
    break; // Purple
  case 5:
    attr = BACKGROUND_GREEN;
    break; // Green
  case 6:
    attr = BACKGROUND_BLUE;
    break; // Blue
  case 7:
    attr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
    break; // Yellow
  case 8:
    attr = BACKGROUND_RED | BACKGROUND_GREEN;
    break; // Orange (Brown-ish)
  case 9:
    attr = BACKGROUND_RED | BACKGROUND_GREEN;
    break; // Brown
  case 10:
    attr = BACKGROUND_RED | BACKGROUND_INTENSITY;
    break; // Lt Red
  case 11:
    attr = BACKGROUND_INTENSITY;
    break; // Grey 1
  case 12:
    attr = BACKGROUND_INTENSITY;
    break; // Grey 2
  case 13:
    attr = BACKGROUND_GREEN | BACKGROUND_INTENSITY;
    break; // Lt Green
  case 14:
    attr = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
    break; // Lt Blue
  case 15:
    attr = BACKGROUND_INTENSITY;
    break; // Grey 3
  }
  // Set text to white (7) on this background
  SetConsoleTextAttribute(hStdout, attr | FOREGROUND_RED | FOREGROUND_GREEN |
                                       FOREGROUND_BLUE);
#else
  // Map C64 colors (0-15) to ANSI colors (simplified)
  int ansi_bg = 40; // Default black
  switch (color & 15) {
  case 0:
    ansi_bg = 40;
    break; // Black
  case 1:
    ansi_bg = 107;
    break; // White
  case 2:
    ansi_bg = 41;
    break; // Red
  case 3:
    ansi_bg = 106;
    break; // Cyan
  case 4:
    ansi_bg = 45;
    break; // Purple
  case 5:
    ansi_bg = 42;
    break; // Green
  case 6:
    ansi_bg = 44;
    break; // Blue
  case 7:
    ansi_bg = 103;
    break; // Yellow
  case 8:
    ansi_bg = 43;
    break; // Orange
  case 9:
    ansi_bg = 101;
    break; // Brown
  case 10:
    ansi_bg = 101;
    break; // Lt Red
  case 11:
    ansi_bg = 100;
    break; // Grey 1
  case 12:
    ansi_bg = 100;
    break; // Grey 2
  case 13:
    ansi_bg = 102;
    break; // Lt Green
  case 14:
    ansi_bg = 104;
    break; // Lt Blue
  case 15:
    ansi_bg = 100;
    break; // Grey 3
  }
  printf("\x1b[%dm", ansi_bg);
#endif
  fflush(stdout);
}

void editor_poke_char(Editor *ed, int addr, uint8_t val) {
  int offset = addr - 1024;
  if (offset < 0 || offset >= 1000)
    return;

  int r = offset / 40;
  int c = offset % 40;

  // Simple CBM core code to ASCII conversion
  char ch = ' ';
  if (val >= 1 && val <= 26)
    ch = val + 64; // A-Z
  else if (val >= 27 && val <= 31)
    ch = val + 64; // [ / ] ^ _
  else if (val >= 32 && val <= 63)
    ch = val; // Space-?
  else if (val >= 64 && val <= 95)
    ch = val + 32; // a-z
  else if (val >= 96 && val <= 127)
    ch = val; // Graphics
  else
    ch = '?'; // Fallback

  // Map to terminal grid (might need scaling)
  int tr = r * ed->rows / 25;
  int tc = c * ed->cols / 40;

  editor_plot(ed, tc, tr, ch);
}

void editor_clear(Editor *ed) { editor_clear_screen(ed); }

void editor_move_cursor(Editor *ed, int row, int col) {
  if (row < 0)
    row = 0;
  if (row >= ed->rows)
    row = ed->rows - 1;
  if (col < 0)
    col = 0;
  if (col >= ed->cols)
    col = ed->cols - 1;

  ed->cursor_row = row;
  ed->cursor_col = col;
  term_move_cursor(row, col);
}

void editor_move_cursor_relative(Editor *ed, int drow, int dcol) {
  int new_row = ed->cursor_row + drow;
  int new_col = ed->cursor_col + dcol;
  editor_move_cursor(ed, new_row, new_col);
}
