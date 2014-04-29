#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

/* Windows */
#ifdef _WIN32

#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

/* Linux */
#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

int main(int argc, char** argv) {
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Integer  = mpc_new("integer");
  mpc_parser_t* Decimal  = mpc_new("decimal");
  mpc_parser_t* OpSymbol = mpc_new("opsymbol");
  mpc_parser_t* OpWord   = mpc_new("opword");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
	    "                                                           \
    decimal  : /-?[0-9]+\\.[0-9]+/ ;					\
    integer  : /-?[0-9]+/ ;						\
    number   : <decimal> | <integer> ;					\
    opsymbol : '+' | '-' | '*' | '/' | '%' ;				\
    opword   : \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ;	\
    operator : <opsymbol> | <opword> ;					\
    expr     : <number> | '(' <expr> <operator> <expr> ')' ;			\
    lispy    : /^/ <expr> <operator> <expr> /$/ ;				\
  ",
	    Decimal, Integer, Number, OpSymbol, OpWord, Operator, Expr, Lispy);


  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  /* In a never ending loop */
  while (1) {

    /* Output our prompt and get input*/
    char* input = readline("lispy> ");

    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On Success Print the AST */
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Free parsers */
  mpc_cleanup(7, Number, Integer, Decimal, OpSymbol, OpWord, Operator, Expr, Lispy);

  return 0;
}