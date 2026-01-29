#ifndef LEXER_H
#define LEXER_H

/* Token types */
typedef enum {
  /* Literals */
  TOK_NUMBER,
  TOK_STRING,
  TOK_IDENTIFIER,

  /* Keywords - Immediate commands */
  TOK_LIST,
  TOK_RUN,
  TOK_NEW,
  TOK_LOAD,
  TOK_SAVE,
  TOK_EXIT,
  TOK_HELP,
  TOK_MEMCHK,
  TOK_CLR,

  /* Keywords - Program statements */
  TOK_PRINT,
  TOK_INPUT,
  TOK_LET,
  TOK_GOTO,
  TOK_GOSUB,
  TOK_RETURN,
  TOK_IF,
  TOK_THEN,
  TOK_ELSE,
  TOK_FOR,
  TOK_TO,
  TOK_STEP,
  TOK_NEXT,
  TOK_DO,
  TOK_LOOP,
  TOK_WHILE,
  TOK_WEND,
  TOK_REPEAT,
  TOK_UNTIL,
  TOK_REM,
  TOK_END,
  TOK_STOP,
  TOK_DIM,
  TOK_TRAP,
  TOK_RESUME,
  TOK_DATA,
  TOK_READ,
  TOK_RESTORE,
  TOK_POKE,
  TOK_PLOT,
  TOK_DRAW,

  /* Operators */
  TOK_PLUS,
  TOK_MINUS,
  TOK_MULTIPLY,
  TOK_DIVIDE,
  TOK_POWER,
  TOK_EQUAL,
  TOK_NOT_EQUAL,
  TOK_LESS,
  TOK_GREATER,
  TOK_LESS_EQUAL,
  TOK_GREATER_EQUAL,
  TOK_AND,
  TOK_OR,
  TOK_NOT,

  /* Built-in functions */
  TOK_ABS,
  TOK_INT,
  TOK_RND,
  TOK_SIN,
  TOK_COS,
  TOK_TAN,
  TOK_SQR,
  TOK_LEN,
  TOK_LEFT,
  TOK_RIGHT,
  TOK_MID,
  TOK_STR,
  TOK_VAL,
  TOK_CHR,
  TOK_ASC,
  TOK_PEEK,

  /* Delimiters */
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_COMMA,
  TOK_SEMICOLON,
  TOK_COLON,
  TOK_QUESTION, /* ? is shorthand for PRINT */

  /* Special */
  TOK_NEWLINE,
  TOK_EOF,
  TOK_ERROR
} TokenType;

typedef struct {
  TokenType type;
  char *text;
  double number_value;
  int line_number;
  int column;
} Token;

typedef struct {
  const char *input;
  int position;
  int line;
  int column;
  Token current_token;
} Lexer;

/* Lexer functions */
void lexer_init(Lexer *lexer, const char *input);
void lexer_free(Lexer *lexer);
Token lexer_next_token(Lexer *lexer);
Token lexer_peek_token(Lexer *lexer);
void token_free(Token *token);
const char *token_type_name(TokenType type);

#endif /* LEXER_H */
