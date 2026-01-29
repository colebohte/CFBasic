#include "utils.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t total_memory_limit = 1073741824; /* 1GB default */
size_t memory_used = 0;

void init_memory(size_t limit) {
  total_memory_limit = limit;
  memory_used = 0;
}

void *safe_malloc(size_t size) {
  size_t total_size = size + sizeof(size_t);
  if (memory_used + total_size > total_memory_limit) {
    error("OUT OF MEMORY");
    return NULL;
  }
  void *ptr = malloc(total_size);
  if (!ptr) {
    error("SYSTEM OUT OF MEMORY");
    exit(1);
  }
  *(size_t *)ptr = size;
  memory_used += total_size;
  return (char *)ptr + sizeof(size_t);
}

void *safe_realloc(void *ptr, size_t old_size, size_t new_size) {
  if (!ptr)
    return safe_malloc(new_size);

  void *real_ptr = (char *)ptr - sizeof(size_t);
  size_t total_new_size = new_size + sizeof(size_t);
  size_t total_old_size = old_size + sizeof(size_t);

  if (memory_used - total_old_size + total_new_size > total_memory_limit) {
    error("OUT OF MEMORY");
    return NULL;
  }

  void *new_ptr = realloc(real_ptr, total_new_size);
  if (!new_ptr) {
    error("SYSTEM OUT OF MEMORY");
    exit(1);
  }

  *(size_t *)new_ptr = new_size;
  memory_used = memory_used - total_old_size + total_new_size;
  return (char *)new_ptr + sizeof(size_t);
}

void safe_free(void *ptr) {
  if (ptr) {
    void *real_ptr = (char *)ptr - sizeof(size_t);
    size_t size = *(size_t *)real_ptr;
    memory_used -= (size + sizeof(size_t));
    free(real_ptr);
  }
}

size_t get_free_memory(void) { return total_memory_limit - memory_used; }

char *str_duplicate(const char *str) {
  if (!str)
    return NULL;
  size_t len = strlen(str) + 1;
  char *dup = safe_malloc(len);
  if (dup) {
    memcpy(dup, str, len);
  }
  return dup;
}

char *str_upper(const char *str) {
  if (!str)
    return NULL;
  size_t len = strlen(str) + 1;
  char *upper = safe_malloc(len);
  if (!upper)
    return NULL;

  for (size_t i = 0; i < len - 1; i++) {
    upper[i] = toupper((unsigned char)str[i]);
  }
  upper[len - 1] = '\0';
  return upper;
}

int str_compare_nocase(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    int c1 = toupper((unsigned char)*s1);
    int c2 = toupper((unsigned char)*s2);
    if (c1 != c2)
      return c1 - c2;
    s1++;
    s2++;
  }
  return toupper((unsigned char)*s1) - toupper((unsigned char)*s2);
}

void error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(stderr, "?");
  vfprintf(stderr, format, args);
  fprintf(stderr, " ERROR\n");
  va_end(args);
}

void warning(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

void clear_screen(void) {
#ifdef _WIN32
  system("cls");
#else
  system("clear");
#endif
}

char *read_line(const char *prompt) {
  if (prompt) {
    printf("%s", prompt);
    fflush(stdout);
  }

  size_t capacity = 128;
  size_t size = 0;
  char *line = safe_malloc(capacity);
  if (!line)
    return NULL;

  int c;
  while ((c = fgetc(stdin)) != EOF && c != '\n') {
    if (size + 1 >= capacity) {
      capacity *= 2;
      char *new_line = safe_realloc(line, capacity / 2, capacity);
      if (!new_line) {
        safe_free(line);
        return NULL;
      }
      line = new_line;
    }
    line[size++] = (char)c;
  }

  if (c == EOF && size == 0) {
    safe_free(line);
    return NULL;
  }

  line[size] = '\0';
  return line;
}

size_t parse_memory_size(const char *str) {
  char *endptr;
  double value = strtod(str, &endptr);

  if (value <= 0) {
    return 0;
  }

  size_t multiplier = 1;
  if (*endptr != '\0') {
    switch (toupper((unsigned char)*endptr)) {
    case 'K':
      multiplier = 1024;
      break;
    case 'M':
      multiplier = 1024 * 1024;
      break;
    case 'G':
      multiplier = 1024 * 1024 * 1024;
      break;
    default:
      return 0; /* Invalid suffix */
    }
  }

  return (size_t)(value * multiplier);
}

void format_memory_size(char *buf, size_t size, size_t limit) {
  const char *units[] = {"B", "KB", "MB", "GB"};
  double used_float = (double)memory_used;
  double free_float = (double)size;
  double limit_float = (double)limit;

  int used_unit = 0;
  while (used_float >= 1024 && used_unit < 3) {
    used_float /= 1024;
    used_unit++;
  }

  int free_unit = 0;
  while (free_float >= 1024 && free_unit < 3) {
    free_float /= 1024;
    free_unit++;
  }

  int limit_unit = 0;
  while (limit_float >= 1024 && limit_unit < 3) {
    limit_float /= 1024;
    limit_unit++;
  }

  sprintf(buf, "%.2f %s FREE, %.2f %s USED, %.0f %s ALLOCATED", free_float,
          units[free_unit], used_float, units[used_unit], limit_float,
          units[limit_unit]);
}
