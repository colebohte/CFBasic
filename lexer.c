#include "lexer.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *keyword;
  TokenType type;
} KeywordMapping;

static KeywordMapping keywords[] = {
    {"LIST", TOK_LIST},       {"RUN", TOK_RUN},       {"NEW", TOK_NEW},
    {"LOAD", TOK_LOAD},       {"SAVE", TOK_SAVE},     {"EXIT", TOK_EXIT},
    {"HELP", TOK_HELP},       {"MEMCHK", TOK_MEMCHK}, {"CLR", TOK_CLR},
    {"PRINT", TOK_PRINT},     {"INPUT", TOK_INPUT},   {"LET", TOK_LET},
    {"GOTO", TOK_GOTO},       {"GOSUB", TOK_GOSUB},   {"RETURN", TOK_RETURN},
    {"IF", TOK_IF},           {"THEN", TOK_THEN},     {"ELSE", TOK_ELSE},
    {"FOR", TOK_FOR},         {"TO", TOK_TO},         {"STEP", TOK_STEP},
    {"NEXT", TOK_NEXT},       {"DO", TOK_DO},         {"LOOP", TOK_LOOP},
    {"WHILE", TOK_WHILE},     {"WEND", TOK_WEND},     {"REPEAT", TOK_REPEAT},
    {"UNTIL", TOK_UNTIL},     {"REM", TOK_REM},       {"END", TOK_END},
    {"STOP", TOK_STOP},       {"DIM", TOK_DIM},       {"TRAP", TOK_TRAP},
    {"RESUME", TOK_RESUME},   {"DATA", TOK_DATA},     {"READ", TOK_READ},
    {"RESTORE", TOK_RESTORE}, {"POKE", TOK_POKE},     {"PLOT", TOK_PLOT},
    {"DRAW", TOK_DRAW},       {"AND", TOK_AND},       {"OR", TOK_OR},
    {"NOT", TOK_NOT},         {"ABS", TOK_ABS},       {"INT", TOK_INT},
    {"RND", TOK_RND},         {"SIN", TOK_SIN},       {"COS", TOK_COS},
    {"TAN", TOK_TAN},         {"SQR", TOK_SQR},       {"LEN", TOK_LEN},
    {"LEFT$", TOK_LEFT},      {"RIGHT$", TOK_RIGHT},  {"MID$", TOK_MID},
    {"STR$", TOK_STR},        {"VAL", TOK_VAL},       {"CHR$", TOK_CHR},
    {"PEEK", TOK_PEEK},       {"ASC", TOK_ASC},       {NULL, TOK_ERROR}};

void lexer_init(Lexer *lexer, const char *input) {
  lexer->input = input;
  lexer->position = 0;
  lexer->line = 1;
  lexer->column = 1;
  lexer->current_token.type = TOK_ERROR;
  lexer->current_token.text = NULL;
}

void lexer_free(Lexer *lexer) {
  if (lexer->current_token.text) {
    safe_free(lexer->current_token.text);
    lexer->current_token.text = NULL;
  }
}

void token_free(Token *token) {
  if (token->text) {
    safe_free(token->text);
    token->text = NULL;
  }
}

static char peek_char(Lexer *lexer) { return lexer->input[lexer->position]; }

static char next_char(Lexer *lexer) {
  char c = lexer->input[lexer->position];
  if (c != '\0') {
    lexer->position++;
    if (c == '\n') {
      lexer->line++;
      lexer->column = 1;
    } else {
      lexer->column++;
    }
  }
  return c;
}

static void skip_whitespace(Lexer *lexer) {
  while (peek_char(lexer) == ' ' || peek_char(lexer) == '\t') {
    next_char(lexer);
  }
}

static Token make_token(TokenType type, const char *text, double number_value,
                        int line, int col) {
  Token token;
  token.type = type;
  token.text = text ? str_duplicate(text) : NULL;
  token.number_value = number_value;
  token.line_number = line;
  token.column = col;
  return token;
}

static Token read_number(Lexer *lexer) {
  int start = lexer->position;
  int line = lexer->line;
  int col = lexer->column;

  while (isdigit(peek_char(lexer))) {
    next_char(lexer);
  }

  if (peek_char(lexer) == '.') {
    next_char(lexer);
    while (isdigit(peek_char(lexer))) {
      next_char(lexer);
    }
  }

  /* Handle scientific notation */
  if (peek_char(lexer) == 'E' || peek_char(lexer) == 'e') {
    next_char(lexer);
    if (peek_char(lexer) == '+' || peek_char(lexer) == '-') {
      next_char(lexer);
    }
    while (isdigit(peek_char(lexer))) {
      next_char(lexer);
    }
  }

  int length = lexer->position - start;
  char *num_str = safe_malloc(length + 1);
  strncpy(num_str, &lexer->input[start], length);
  num_str[length] = '\0';

  double value = atof(num_str);
  Token token = make_token(TOK_NUMBER, num_str, value, line, col);
  safe_free(num_str);

  return token;
}

static Token read_string(Lexer *lexer) {
  int line = lexer->line;
  int col = lexer->column;

  next_char(lexer); /* Skip opening quote */

  int start = lexer->position;
  while (peek_char(lexer) != '"' && peek_char(lexer) != '\0' &&
         peek_char(lexer) != '\n') {
    next_char(lexer);
  }

  int length = lexer->position - start;
  char *str = safe_malloc(length + 1);
  strncpy(str, &lexer->input[start], length);
  str[length] = '\0';

  if (peek_char(lexer) == '"') {
    next_char(lexer); /* Skip closing quote */
  }

  Token token = make_token(TOK_STRING, str, 0, line, col);
  safe_free(str);

  return token;
}

static Token read_identifier(Lexer *lexer) {
  int start = lexer->position;
  int line = lexer->line;
  int col = lexer->column;

  while (isalnum(peek_char(lexer)) || peek_char(lexer) == '_' ||
         peek_char(lexer) == '$') {
    next_char(lexer);
  }

  int length = lexer->position - start;
  char *ident = safe_malloc(length + 1);
  strncpy(ident, &lexer->input[start], length);
  ident[length] = '\0';

  /* Convert to uppercase for keyword matching */
  char *upper = str_upper(ident);

  /* Check if it's a keyword */
  TokenType type = TOK_IDENTIFIER;
  for (int i = 0; keywords[i].keyword != NULL; i++) {
    if (strcmp(upper, keywords[i].keyword) == 0) {
      type = keywords[i].type;
      break;
    }
  }

  Token token = make_token(type, ident, 0, line, col);
  safe_free(ident);
  safe_free(upper);

  return token;
}

Token lexer_next_token(Lexer *lexer) {
  skip_whitespace(lexer);

  char c = peek_char(lexer);
  int line = lexer->line;
  int col = lexer->column;

  if (c == '\0') {
    return make_token(TOK_EOF, NULL, 0, line, col);
  }

  if (c == '\n' || c == '\r') {
    next_char(lexer);
    if (c == '\r' && peek_char(lexer) == '\n') {
      next_char(lexer);
    }
    return make_token(TOK_NEWLINE, NULL, 0, line, col);
  }

  if (isdigit(c)) {
    return read_number(lexer);
  }

  if (c == '"') {
    return read_string(lexer);
  }

  if (isalpha(c)) {
    return read_identifier(lexer);
  }

  /* Single character tokens */
  next_char(lexer);

  switch (c) {
  case '+':
    return make_token(TOK_PLUS, "+", 0, line, col);
  case '-':
    return make_token(TOK_MINUS, "-", 0, line, col);
  case '*':
    return make_token(TOK_MULTIPLY, "*", 0, line, col);
  case '/':
    return make_token(TOK_DIVIDE, "/", 0, line, col);
  case '^':
    return make_token(TOK_POWER, "^", 0, line, col);
  case '(':
    return make_token(TOK_LPAREN, "(", 0, line, col);
  case ')':
    return make_token(TOK_RPAREN, ")", 0, line, col);
  case ',':
    return make_token(TOK_COMMA, ",", 0, line, col);
  case ';':
    return make_token(TOK_SEMICOLON, ";", 0, line, col);
  case ':':
    return make_token(TOK_COLON, ":", 0, line, col);
  case '?':
    return make_token(TOK_QUESTION, "?", 0, line, col);
  case '=':
    return make_token(TOK_EQUAL, "=", 0, line, col);
  case '<':
    if (peek_char(lexer) == '=') {
      next_char(lexer);
      return make_token(TOK_LESS_EQUAL, "<=", 0, line, col);
    } else if (peek_char(lexer) == '>') {
      next_char(lexer);
      return make_token(TOK_NOT_EQUAL, "<>", 0, line, col);
    }
    return make_token(TOK_LESS, "<", 0, line, col);
  case '>':
    if (peek_char(lexer) == '=') {
      next_char(lexer);
      return make_token(TOK_GREATER_EQUAL, ">=", 0, line, col);
    }
    return make_token(TOK_GREATER, ">", 0, line, col);
  }

  return make_token(TOK_ERROR, NULL, 0, line, col);
}

Token lexer_peek_token(Lexer *lexer) {
  int pos = lexer->position;
  int line = lexer->line;
  int col = lexer->column;
  Token token = lexer_next_token(lexer);
  lexer->position = pos;
  lexer->line = line;
  lexer->column = col;
  return token;
}

const char *token_type_name(TokenType type) {
  switch (type) {
  case TOK_NUMBER:
    return "NUMBER";
  case TOK_STRING:
    return "STRING";
  case TOK_IDENTIFIER:
    return "IDENTIFIER";
  case TOK_PRINT:
    return "PRINT";
  case TOK_IF:
    return "IF";
  case TOK_THEN:
    return "THEN";
  case TOK_ELSE:
    return "ELSE";
  case TOK_FOR:
    return "FOR";
  case TOK_NEXT:
    return "NEXT";
  case TOK_GOTO:
    return "GOTO";
  case TOK_GOSUB:
    return "GOSUB";
  case TOK_RETURN:
    return "RETURN";
  case TOK_EOF:
    return "EOF";
  case TOK_NEWLINE:
    return "NEWLINE";
  default:
    return "UNKNOWN";
  }
}
