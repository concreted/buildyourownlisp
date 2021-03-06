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

/* Setup */
enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct {
  int type;
  long num; 
  int err;
} lval;

/* lval_num constructor */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* lval_err constructor */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* Print an lval */
void lval_print(lval v) {
  switch (v.type) {
  case LVAL_NUM: printf("%li", v.num); break;
  case LVAL_ERR:
    if (v.err == LERR_DIV_ZERO) { printf("Error: Division by Zero!"); }
    if (v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator!"); }
    if (v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number!"); }
    break;
  }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

/* Compute numbers of leaves in a tree */
int count_leaves(mpc_ast_t* t) {
  /*
  printf("Tag: %s\n", t->tag);
  printf("Contents: %s\n", t->contents);
  printf("Number of children: %i\n", t->children_num);
  */
  
  if (t->children_num == 0) { return 1; }
  if (t->children_num >= 1) {
    int total = 0;
    for (int i = 0; i < t->children_num; i++) {
      total = total + count_leaves(t->children[i]);
    }
    return total;
  }
  return -1;
}

/* Compute number of branches in a tree */
int count_branches(mpc_ast_t* t) {
  //printf("contents: %s\n", t->contents);
  if (t->children_num == 0) { return 0; }
  if (t->children_num >= 1) {
    int total = 0;
    if (strcmp(t->contents, "") == 0) { total = 1; }
    for (int i = 0; i < t->children_num; i++) {
      total = total + count_branches(t->children[i]); 
    }
    return total;
  }
  return -1;
}

/* Compute most number of children from one branch in a tree */ 
int most_leaves_in_branch(mpc_ast_t* t) {
  return 0;
}


/* Use operator string to see which operation to perform */
lval eval_op(lval x, char* op, lval y) {

  /* Handle errors */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }
  
  /* Perform operations based on operator */
  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "%") == 0) { return lval_num(x.num % y.num); }
  if (strcmp(op, "/") == 0) { 
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }
  
  return lval_err(LERR_BAD_OP);
}

/* Evaluate unary operation using operator string */
lval eval_op_unary(lval x, char* op) {
  if (strcmp(op, "-") == 0) { return lval_num(-x.num); }
  return lval_err(LERR_BAD_OP);
}

/* Evaluate tree */
lval eval(mpc_ast_t* t) {
  
  /* If tagged as number return it directly, otherwise expression. */ 
  if (strstr(t->tag, "number")) { 
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }
  
  /* The operator is always second child. */
  char* op = t->children[1]->contents;
  
  /* We store the third child in `x` */
  lval x = eval(t->children[2]);
  
  /* Iterate the remaining children, combining using our operator */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  /* Evaluate unary operators if no other children */
  if (i == 3) { x = eval_op_unary(x, op); }

  return x;  
}

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
    expr     : <number> | '(' <operator> <expr>+ ')' ;			\
    lispy    : /^/ <operator> <expr>+ /$/ ;				\
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

      lval result = eval(r.output);
      lval_println(result);

      printf("Leaves: %i\n", count_leaves(r.output));
      printf("Branches: %i\n", count_branches(r.output));

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
