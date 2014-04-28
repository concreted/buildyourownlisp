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
  mpc_parser_t* AllAB   = mpc_new("allab");
  mpc_parser_t* Grammar = mpc_new("grammar");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
  "                                                     \
    allab   : /[ab]*/ ;					\
    grammar : /^/ <allab> /$/ ;					\
  ",
	    AllAB, Grammar);


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
    if (mpc_parse("<stdin>", input, Grammar, &r)) {
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
  mpc_cleanup(2, AllAB, Grammar);

  return 0;
}
