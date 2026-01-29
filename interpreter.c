#define _GNU_SOURCE
#include "interpreter.h"
#include "editor.h"
#include "lexer.h"
#include "utils.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void basic_print(Interpreter *interp, const char *format, ...) {
  va_list args;
  va_start(args, format);
  char *buf = NULL;
  if (vasprintf(&buf, format, args) != -1) {
    if (interp->editor) {
      editor_print(interp->editor, buf);
    } else {
      printf("%s", buf);
      fflush(stdout);
    }
    free(buf);
  }
  va_end(args);
}

static void interpreter_error(Interpreter *interp, const char *msg) {
  interp->error_occurred = true;
  if (interp->error_message) {
    safe_free(interp->error_message);
  }
  interp->error_message = str_duplicate(msg);
}

void interpreter_init(Interpreter *interp) {
  interp->program = NULL;
  interp->current_line = NULL;
  interp->variables = NULL;
  interp->call_stack = NULL;
  interp->for_stack = NULL;
  interp->editor = NULL; // Initialize
  interp->running = false;
  interp->break_requested = false;
  interp->exit_requested = false;
  interp->error_occurred = false;
  interp->graphics_x = 0;
  interp->graphics_y = 0;
  memset(interp->ram, 0, sizeof(interp->ram));
  interp->error_message = NULL;

  /* Seed random number generator */
  srand(time(NULL));
}

void interpreter_free(Interpreter *interp) {
  program_clear(interp);
  var_clear_all(interp);

  while (interp->call_stack) {
    stack_pop(interp);
  }

  while (interp->for_stack) {
    for_pop(interp);
  }

  if (interp->error_message) {
    safe_free(interp->error_message);
  }
}

/* Program line management */
void program_add_line(Interpreter *interp, int line_num, const char *text) {
  /* Delete existing line with same number */
  program_delete_line(interp, line_num);

  /* If text is empty, just delete the line */
  if (!text || strlen(text) == 0) {
    return;
  }

  /* Create new line */
  ProgramLine *new_line = safe_malloc(sizeof(ProgramLine));
  new_line->line_number = line_num;
  new_line->text = str_duplicate(text);
  new_line->next = NULL;

  /* Insert in sorted order */
  if (!interp->program || interp->program->line_number > line_num) {
    new_line->next = interp->program;
    interp->program = new_line;
  } else {
    ProgramLine *current = interp->program;
    while (current->next && current->next->line_number < line_num) {
      current = current->next;
    }
    new_line->next = current->next;
    current->next = new_line;
  }
}

void program_delete_line(Interpreter *interp, int line_num) {
  if (!interp->program)
    return;

  if (interp->program->line_number == line_num) {
    ProgramLine *temp = interp->program;
    interp->program = interp->program->next;
    safe_free(temp->text);
    safe_free(temp);
    return;
  }

  ProgramLine *current = interp->program;
  while (current->next) {
    if (current->next->line_number == line_num) {
      ProgramLine *temp = current->next;
      current->next = temp->next;
      safe_free(temp->text);
      safe_free(temp);
      return;
    }
    current = current->next;
  }
}

ProgramLine *program_find_line(Interpreter *interp, int line_num) {
  ProgramLine *current = interp->program;
  while (current) {
    if (current->line_number == line_num) {
      return current;
    }
    if (current->line_number > line_num) {
      return NULL;
    }
    current = current->next;
  }
  return NULL;
}

void program_clear(Interpreter *interp) {
  while (interp->program) {
    ProgramLine *temp = interp->program;
    interp->program = interp->program->next;
    safe_free(temp->text);
    safe_free(temp);
  }
}

/* Variable management */
Variable *var_get(Interpreter *interp, const char *name) {
  Variable *current = interp->variables;
  while (current) {
    if (str_compare_nocase(current->name, name) == 0) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

Variable *var_set_number(Interpreter *interp, const char *name, double value) {
  Variable *var = var_get(interp, name);

  if (!var) {
    var = safe_malloc(sizeof(Variable));
    var->name = str_duplicate(name);
    var->type = VAR_NUMBER;
    var->value.number = value;
    var->next = interp->variables;
    interp->variables = var;
  } else {
    if (var->type == VAR_STRING && var->value.string) {
      safe_free(var->value.string);
    }
    var->type = VAR_NUMBER;
    var->value.number = value;
  }

  return var;
}

Variable *var_set_string(Interpreter *interp, const char *name,
                         const char *value) {
  Variable *var = var_get(interp, name);

  if (!var) {
    var = safe_malloc(sizeof(Variable));
    var->name = str_duplicate(name);
    var->type = VAR_STRING;
    var->value.string = str_duplicate(value);
    var->next = interp->variables;
    interp->variables = var;
  } else {
    if (var->type == VAR_STRING && var->value.string) {
      safe_free(var->value.string);
    }
    var->type = VAR_STRING;
    var->value.string = str_duplicate(value);
  }

  return var;
}

void var_clear_all(Interpreter *interp) {
  while (interp->variables) {
    Variable *temp = interp->variables;
    interp->variables = interp->variables->next;

    if (temp->type == VAR_STRING && temp->value.string) {
      safe_free(temp->value.string);
    }
    safe_free(temp->name);
    safe_free(temp);
  }
}

/* Stack management for GOSUB/RETURN */
void stack_push(Interpreter *interp, int return_line) {
  StackFrame *frame = safe_malloc(sizeof(StackFrame));
  frame->return_line = return_line;
  frame->next = interp->call_stack;
  interp->call_stack = frame;
}

int stack_pop(Interpreter *interp) {
  if (!interp->call_stack) {
    interpreter_error(interp, "RETURN WITHOUT GOSUB");
    return 0; // Return a dummy value, as error will stop execution
  }

  StackFrame *frame = interp->call_stack;
  int return_line = frame->return_line;
  interp->call_stack = frame->next;
  safe_free(frame);

  return return_line;
}

/* FOR loop management */
void for_push(Interpreter *interp, const char *var_name, double end,
              double step, int line) {
  ForLoop *loop = safe_malloc(sizeof(ForLoop));
  loop->var_name = str_duplicate(var_name);
  loop->end_value = end;
  loop->step_value = step;
  loop->loop_line = line;
  loop->next = interp->for_stack;
  interp->for_stack = loop;
}

ForLoop *for_find(Interpreter *interp, const char *var_name) {
  ForLoop *current = interp->for_stack;
  while (current) {
    if (str_compare_nocase(current->var_name, var_name) == 0) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

void for_pop(Interpreter *interp) {
  if (!interp->for_stack)
    return;

  ForLoop *loop = interp->for_stack;
  interp->for_stack = loop->next;
  safe_free(loop->var_name);
  safe_free(loop);
}

/* Interpreter commands */
void interpreter_list(Interpreter *interp, int start, int end) {
  ProgramLine *current = interp->program;

  while (current) {
    if (current->line_number >= start &&
        (end == -1 || current->line_number <= end)) {
      basic_print(interp, "%d %s\n", current->line_number, current->text);
    }
    current = current->next;
  }
}

void interpreter_new(Interpreter *interp) {
  program_clear(interp);
  var_clear_all(interp);

  while (interp->call_stack) {
    stack_pop(interp);
  }

  while (interp->for_stack) {
    for_pop(interp);
  }
}

bool interpreter_load(Interpreter *interp, const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    interpreter_error(interp, "FILE NOT FOUND");
    return false;
  }

  interpreter_new(interp);

  char line[1024];
  while (fgets(line, sizeof(line), file)) {
    /* Remove newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    /* Parse line number */
    int line_num;
    char text[1024];
    if (sscanf(line, "%d %[^\n]", &line_num, text) == 2) {
      program_add_line(interp, line_num, text);
    }
  }

  fclose(file);
  return true;
}

bool interpreter_save(Interpreter *interp, const char *filename) {
  FILE *file = fopen(filename, "w");
  if (!file) {
    interpreter_error(interp, "CANNOT SAVE FILE");
    return false;
  }

  ProgramLine *current = interp->program;
  while (current) {
    fprintf(file, "%d %s\n", current->line_number, current->text);
    current = current->next;
  }

  fclose(file);
  return true;
}

void interpreter_run(Interpreter *interp) {
  if (!interp->program) {
    return;
  }

  interp->running = true;
  interp->current_line = interp->program;

  while (interp->running && interp->current_line) {
    if (interp->break_requested) {
      basic_print(interp, "\n? BREAK\n");
      interp->break_requested = false;
      interp->running = false;
      break;
    }

    ProgramLine *executing_line = interp->current_line;
    interpreter_execute_line(interp, executing_line->text);

    if (interp->error_occurred) {
      if (interp->error_message) {
        basic_print(interp, "?%s ERROR IN %d\n", interp->error_message,
                    executing_line->line_number);
        safe_free(interp->error_message);
        interp->error_message = NULL;
      } else {
        basic_print(interp, "?ERROR IN %d\n", executing_line->line_number);
      }
      interp->running = false;
      interp->error_occurred = false;
      break;
    }

    /* Advance if execution didn't change current_line */
    if (interp->running && interp->current_line == executing_line) {
      interp->current_line = executing_line->next;
    }
  }

  interp->running = false;
}

/* Very basic value structure for expressions */
typedef struct {
  bool is_string;
  double number;
  char *string;
} Value;

Value evaluate_expression(Interpreter *interp, Lexer *lexer);

Value evaluate_factor(Interpreter *interp, Lexer *lexer) {
  Token token = lexer_next_token(lexer);
  Value val = {false, 0, NULL};

  if (token.type == TOK_NUMBER) {
    val.is_string = false;
    val.number = token.number_value;
  } else if (token.type == TOK_STRING) {
    val.is_string = true;
    val.string = str_duplicate(token.text);
  } else if (token.type == TOK_IDENTIFIER) {
    Variable *v = var_get(interp, token.text);
    if (v) {
      if (v->type == VAR_NUMBER) {
        val.is_string = false;
        val.number = v->value.number;
      } else if (v->type == VAR_STRING) {
        val.is_string = true;
        val.string = str_duplicate(v->value.string);
      }
    } else {
      // Default to 0 or empty string if not found
      if (token.text && token.text[strlen(token.text) - 1] == '$') {
        val.is_string = true;
        val.string = str_duplicate("");
      } else {
        val.is_string = false;
        val.number = 0;
      }
    }
  } else if (token.type == TOK_LPAREN) {
    token_free(&token);
    val = evaluate_expression(interp, lexer);
    Token rparen = lexer_next_token(lexer);
    token_free(&rparen);
    return val;
  } else if (token.type == TOK_CHR) {
    token_free(&token);
    Token lparen = lexer_next_token(lexer);
    token_free(&lparen);
    Value v = evaluate_expression(interp, lexer);
    Token rparen = lexer_next_token(lexer);
    token_free(&rparen);

    val.is_string = true;
    char buf[2] = {(char)v.number, 0};
    val.string = str_duplicate(buf);
    if (v.is_string)
      safe_free(v.string);
  } else if (token.type == TOK_PEEK) {
    token_free(&token);
    Token lparen = lexer_next_token(lexer);
    token_free(&lparen);
    Value v = evaluate_expression(interp, lexer);
    Token rparen = lexer_next_token(lexer);
    token_free(&rparen);

    val.is_string = false;
    uint16_t addr = (uint16_t)v.number;
    val.number = interp->ram[addr];
    if (v.is_string)
      safe_free(v.string);
  }

  token_free(&token);
  return val;
}

Value evaluate_expression(Interpreter *interp, Lexer *lexer) {
  /* Simplified: handles addition/concatenation and comparisons */
  Value left = evaluate_factor(interp, lexer);

  while (true) {
    Token peek = lexer_peek_token(lexer);
    if (peek.type == TOK_PLUS) {
      lexer_next_token(lexer); // consume +
      Value right = evaluate_factor(interp, lexer);
      if (left.is_string && right.is_string) {
        char *new_str =
            safe_malloc(strlen(left.string) + strlen(right.string) + 1);
        strcpy(new_str, left.string);
        strcat(new_str, right.string);
        safe_free(left.string);
        safe_free(right.string);
        left.string = new_str;
      } else if (!left.is_string && !right.is_string) {
        left.number += right.number;
      }
      token_free(&peek);
    } else if (peek.type == TOK_EQUAL || peek.type == TOK_NOT_EQUAL ||
               peek.type == TOK_LESS || peek.type == TOK_GREATER ||
               peek.type == TOK_LESS_EQUAL || peek.type == TOK_GREATER_EQUAL) {
      lexer_next_token(lexer); // consume operator
      Value right = evaluate_factor(interp, lexer);
      double res = 0;

      if (left.is_string && right.is_string) {
        int cmp = strcmp(left.string, right.string);
        if (peek.type == TOK_EQUAL)
          res = (cmp == 0);
        else if (peek.type == TOK_NOT_EQUAL)
          res = (cmp != 0);
        else if (peek.type == TOK_LESS)
          res = (cmp < 0);
        else if (peek.type == TOK_GREATER)
          res = (cmp > 0);
        else if (peek.type == TOK_LESS_EQUAL)
          res = (cmp <= 0);
        else if (peek.type == TOK_GREATER_EQUAL)
          res = (cmp >= 0);
        safe_free(left.string);
        safe_free(right.string);
      } else if (!left.is_string && !right.is_string) {
        if (peek.type == TOK_EQUAL)
          res = (left.number == right.number);
        else if (peek.type == TOK_NOT_EQUAL)
          res = (left.number != right.number);
        else if (peek.type == TOK_LESS)
          res = (left.number < right.number);
        else if (peek.type == TOK_GREATER)
          res = (left.number > right.number);
        else if (peek.type == TOK_LESS_EQUAL)
          res = (left.number <= right.number);
        else if (peek.type == TOK_GREATER_EQUAL)
          res = (left.number >= right.number);
      }

      left.is_string = false;
      left.number = res ? -1 : 0; // BASIC true is -1
      token_free(&peek);
    } else {
      token_free(&peek);
      break;
    }
  }
  return left;
}

static void draw_line(Interpreter *interp, int x1, int y1, int x2, int y2) {
  if (!interp->editor)
    return;

  // Scale from C64/C128 resolution (320x200) to terminal size
  int tx1 = x1 * interp->editor->cols / 320;
  int ty1 = y1 * interp->editor->rows / 200;
  int tx2 = x2 * interp->editor->cols / 320;
  int ty2 = y2 * interp->editor->rows / 200;

  int dx = abs(tx2 - tx1);
  int dy = abs(ty2 - ty1);
  int sx = (tx1 < tx2) ? 1 : -1;
  int sy = (ty1 < ty2) ? 1 : -1;
  int err = dx - dy;

  while (1) {
    editor_plot(interp->editor, tx1, ty1, '*');
    if (tx1 == tx2 && ty1 == ty2)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      tx1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      ty1 += sy;
    }
  }
}

void interpreter_execute_line(Interpreter *interp, const char *line) {
  Lexer lexer;
  lexer_init(&lexer, line);

  while (true) {
    Token token = lexer_next_token(&lexer);

    if (token.type == TOK_EOF || token.type == TOK_NEWLINE) {
      token_free(&token);
      break;
    }

    if (token.type == TOK_PRINT || token.type == TOK_QUESTION) {
      token_free(&token);
      while (true) {
        Token peek = lexer_peek_token(&lexer);
        if (peek.type == TOK_EOF || peek.type == TOK_NEWLINE ||
            peek.type == TOK_COLON) {
          basic_print(interp, "\n");
          token_free(&peek);
          break;
        }
        token_free(&peek);

        Value v = evaluate_expression(interp, &lexer);
        if (v.is_string) {
          /* Handle some CBM control characters */
          if (v.string) {
            for (char *p = v.string; *p; p++) {
              unsigned char c = (unsigned char)*p;
              if (interp->editor) {
                if (c == 147) { // CLR/HOME
                  editor_clear(interp->editor);
                } else if (c == 19) { // HOME
                  editor_move_cursor(interp->editor, 0, 0);
                } else if (c == 17) { // CSR DOWN
                  editor_move_cursor_relative(interp->editor, 1, 0);
                } else if (c == 145) { // CSR UP
                  editor_move_cursor_relative(interp->editor, -1, 0);
                } else if (c == 157) { // CSR LEFT
                  editor_move_cursor_relative(interp->editor, 0, -1);
                } else if (c == 29) { // CSR RIGHT
                  editor_move_cursor_relative(interp->editor, 0, 1);
                } else {
                  char buf[2] = {(char)c, 0};
                  editor_print(interp->editor, buf);
                }
              } else {
                if (c == 147) { // CLR/HOME
                  printf("\x1b[2J\x1b[H");
                } else if (c == 19) { // HOME
                  printf("\x1b[H");
                } else if (c == 17) { // CSR DOWN
                  printf("\x1b[B");
                } else if (c == 145) { // CSR UP
                  printf("\x1b[A");
                } else if (c == 157) { // CSR LEFT
                  printf("\x1b[D");
                } else if (c == 29) { // CSR RIGHT
                  printf("\x1b[C");
                } else {
                  putchar(c);
                }
                fflush(stdout);
              }
            }
          }
          safe_free(v.string);
        } else {
          basic_print(interp, "%g", v.number);
        }

        peek = lexer_peek_token(&lexer);
        if (peek.type == TOK_SEMICOLON) {
          lexer_next_token(&lexer);
          token_free(&peek);
        } else if (peek.type == TOK_COMMA) {
          lexer_next_token(&lexer);
          token_free(&peek);
          basic_print(interp, "\t");
        } else {
          basic_print(interp, "\n");
          token_free(&peek);
          break;
        }
      }
    } else if (token.type == TOK_IF) {
      token_free(&token);
      Value cond = evaluate_expression(interp, &lexer);
      Token then_tok = lexer_next_token(&lexer);

      if (then_tok.type == TOK_THEN) {
        if (cond.number != 0) {
          /* THEN branch */
          Token peek = lexer_peek_token(&lexer);
          if (peek.type == TOK_NUMBER) {
            /* IF...THEN [line] */
            lexer_next_token(&lexer); // consume number
            ProgramLine *target =
                program_find_line(interp, (int)peek.number_value);
            if (target) {
              interp->current_line = target;
              // Important: break the token loop so we don't execute rest of
              // this line
              token_free(&peek);
              token_free(&then_tok);
              if (cond.is_string)
                safe_free(cond.string);
              break;
            }
            token_free(&peek);
          }
          token_free(&peek);
          /* IF...THEN [statement] -- just continue execution */
        } else {
          /* Skip to ELSE or end of line */
          while (true) {
            token = lexer_next_token(&lexer);
            if (token.type == TOK_EOF || token.type == TOK_NEWLINE)
              break;
            if (token.type == TOK_ELSE)
              break;
            token_free(&token);
          }
        }
      }
      token_free(&then_tok);
      if (cond.is_string)
        safe_free(cond.string);
    } else if (token.type == TOK_GOTO) {
      token_free(&token);
      Value v = evaluate_expression(interp, &lexer);
      if (!v.is_string) {
        ProgramLine *target = program_find_line(interp, (int)v.number);
        if (target) {
          interp->current_line = target;
        } else {
          interpreter_error(interp, "LINE NOT FOUND");
        }
      }
      if (v.is_string)
        safe_free(v.string);
    } else if (token.type == TOK_GOSUB) {
      token_free(&token);
      Value v = evaluate_expression(interp, &lexer);
      if (!v.is_string) {
        ProgramLine *target = program_find_line(interp, (int)v.number);
        if (target) {
          if (interp->current_line) {
            stack_push(interp, interp->current_line->line_number);
          }
          interp->current_line = target;
        } else {
          interpreter_error(interp, "LINE NOT FOUND");
        }
      }
      if (v.is_string)
        safe_free(v.string);
    } else if (token.type == TOK_RETURN) {
      token_free(&token);
      int return_line = stack_pop(interp);
      if (!interp->error_occurred) {
        ProgramLine *target = program_find_line(interp, return_line);
        if (target) {
          interp->current_line = target->next;
        } else {
          // If the line was deleted, find the next one
          // This is a bit complex, but standard BASIC behavior is usually
          // to error or jump to next available.
          // For now, let's just find the first line > return_line.
          ProgramLine *curr = interp->program;
          while (curr && curr->line_number <= return_line) {
            curr = curr->next;
          }
          interp->current_line = curr;
        }
      }
    } else if (token.type == TOK_LET || token.type == TOK_IDENTIFIER) {
      char *name = NULL;
      if (token.type == TOK_IDENTIFIER) {
        name = str_duplicate(token.text);
      } else {
        token_free(&token);
        token = lexer_next_token(&lexer);
        if (token.type == TOK_IDENTIFIER) {
          name = str_duplicate(token.text);
        }
      }
      token_free(&token);

      if (name) {
        Token eq = lexer_next_token(&lexer);
        if (eq.type == TOK_EQUAL) {
          Value v = evaluate_expression(interp, &lexer);
          if (v.is_string) {
            var_set_string(interp, name, v.string);
            safe_free(v.string);
          } else {
            var_set_number(interp, name, v.number);
          }
        } else {
          interpreter_error(interp, "SYNTAX");
        }
        token_free(&eq);
        safe_free(name);
      }
    } else if (token.type == TOK_POKE) {
      token_free(&token);
      Value addr = evaluate_expression(interp, &lexer);
      Token comma = lexer_next_token(&lexer);
      token_free(&comma);
      Value val = evaluate_expression(interp, &lexer);

      if (!addr.is_string && !val.is_string) {
        uint16_t a = (uint16_t)addr.number;
        uint8_t v = (uint8_t)val.number;
        interp->ram[a] = v;

        if (interp->editor) {
          if (a == 53280 || a == 53281) {
            editor_set_background_color(interp->editor, v);
          } else if (a >= 1024 && a <= 2023) {
            editor_poke_char(interp->editor, a, v);
          }
        }
      }
      if (addr.is_string)
        safe_free(addr.string);
      if (val.is_string)
        safe_free(val.string);
    } else if (token.type == TOK_PLOT) {
      token_free(&token);
      Value vx = evaluate_expression(interp, &lexer);
      Token comma = lexer_next_token(&lexer);
      token_free(&comma);
      Value vy = evaluate_expression(interp, &lexer);
      if (!vx.is_string && !vy.is_string) {
        interp->graphics_x = vx.number;
        interp->graphics_y = vy.number;
      }
      if (vx.is_string)
        safe_free(vx.string);
      if (vy.is_string)
        safe_free(vy.string);
    } else if (token.type == TOK_DRAW) {
      token_free(&token);
      Value vx = evaluate_expression(interp, &lexer);
      Token comma = lexer_next_token(&lexer);
      token_free(&comma);
      Value vy = evaluate_expression(interp, &lexer);
      if (!vx.is_string && !vy.is_string) {
        draw_line(interp, (int)interp->graphics_x, (int)interp->graphics_y,
                  (int)vx.number, (int)vy.number);
        interp->graphics_x = vx.number;
        interp->graphics_y = vy.number;
      }
      if (vx.is_string)
        safe_free(vx.string);
      if (vy.is_string)
        safe_free(vy.string);
    } else if (token.type == TOK_EXIT) {
      interp->exit_requested = true;
      interp->running = false;
      token_free(&token);
      break;
    } else if (token.type == TOK_END || token.type == TOK_STOP) {
      interp->running = false;
      token_free(&token);
      break;
    } else if (token.type == TOK_COLON) {
      token_free(&token);
      continue;
    } else if (token.type == TOK_CLR) {
      if (interp->editor) {
        editor_clear(interp->editor);
      } else {
        clear_screen();
      }
      token_free(&token);
    } else if (token.type == TOK_MEMCHK) {
      char mem_buf[256];
      format_memory_size(mem_buf, get_free_memory(), total_memory_limit);
      for (int i = 0; mem_buf[i]; i++) {
        mem_buf[i] = toupper((unsigned char)mem_buf[i]);
      }
      basic_print(interp, "%s\n", mem_buf);
      token_free(&token);
    } else if (token.type == TOK_REM) {
      token_free(&token);
      break;
    } else {
      interpreter_error(interp, "SYNTAX");
      token_free(&token);
      break;
    }

    if (interp->error_occurred)
      break;
  }

  lexer_free(&lexer);
}
