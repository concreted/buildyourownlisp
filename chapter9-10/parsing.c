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

/* Macros */
#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

/* Setup */
enum { LVAL_INT, LVAL_DEC, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct lval {
  int type;
  double num;

  char* err;
  char* sym;
  
  int count;
  struct lval** cell;

} lval;

/* lval_num constructor */
lval* lval_num(double x) {
  lval* v = malloc(sizeof(lval));
  if ((long) x == x)
    v->type = LVAL_INT;
  else
    v->type = LVAL_DEC;
  v->num = x;
  return v;
}

/* lval_err constructor */
lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

/* lval_sym constructor */
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* Delete an lval */
void lval_del(lval* v) {

  switch (v->type) {
    /* Do nothing special for number type */
  case LVAL_INT: break;

    /* For Err or Sym free the string data */
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;

    /* If Sexpr then delete all elements inside */
  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    /* Also free the memory allocated to contain the pointers */
    free(v->cell);
    break;
  }

  /* Finally free the memory allocated for the "lval" struct itself */
  free(v);
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {

  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/* PRINTING */

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    /* Print Value contained within */
    lval_print(v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
  case LVAL_INT:   printf("%li", (long) v->num); break;
  case LVAL_DEC:   printf("%.2f", v->num); break;
  case LVAL_ERR:   printf("Error: %s", v->err); break;
  case LVAL_SYM:   printf("%s", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/* EVALUATION */
lval* lval_eval(lval* v);

lval* lval_pop(lval* v, int i) {
  /* Find the item at "i" */
  lval* x = v->cell[i];

  /* Shift the memory following the item at "i" over the top of it */
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_head(lval* a) {
  /* Check Error Conditions */
  LASSERT(a, (a->count == 1                 ), "Function 'head' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type!");
  LASSERT(a, (a->cell[0]->count != 0        ), "Function 'head' passed {}!");

  /* Otherwise take first argument */
  lval* v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lval* a) {
  /* Check Error Conditions */
  LASSERT(a, (a->count == 1                 ), "Function 'tail' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect type!");
  LASSERT(a, (a->cell[0]->count != 0        ), "Function 'tail' passed {}!");

  /* Take first argument */
  lval* v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval* a) {
  LASSERT(a, (a->count == 1                 ), "Function 'eval' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* lval_join(lval* x, lval* y) {

  /* For each cell in 'y' add it to 'x' */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* Delete the empty 'y' and return 'x' */
  lval_del(y);  
  return x;
}

lval* builtin_join(lval* a) {

  for (int i = 0; i < a->count; i++) {
    LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type.");
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_op(lval* a, char* op) {
  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_INT && a->cell[i]->type != LVAL_DEC) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }
  
  /* Pop the first element */
  lval* x = lval_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) { 
    x->num = -x->num; 
  }

  /* While there are still elements remaining */
  while (a->count > 0) {

    /* Pop the next element */
    lval* y = lval_pop(a, 0);

    /* Perform operation */
    if (strcmp(op, "+") == 0) { 
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0) { 
      x->num -= y->num; 
    }
    if (strcmp(op, "*") == 0) { 
      x->num *= y->num; 
    }
    if (strcmp(op, "%") == 0) { 
      x->num = fmod(x->num, y->num); 
    }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0.0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero!"); break;
      }
      x->num /= y->num;
    }

    /* Delete element now finished with */
    lval_del(y);
  }

  /* Delete input expression and return result */
  lval_del(a);

  if (x->num == (long) x->num) 
    x->type = LVAL_INT;
  else
    x->type = LVAL_DEC;

  return x;
}

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-/*", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("Unknown Function!");
}

lval* lval_eval_sexpr(lval* v) {

  /* Evaluate Children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* Error Checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* Empty Expression */
  if (v->count == 0) { return v; }

  /* Single Expression */
  if (v->count == 1) { return lval_take(v, 0); }

  /* Ensure First Element is Symbol */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f); lval_del(v);
    return lval_err("S-expression Does not start with symbol!");
  }

  /* Call builtin with operator */
  lval* result = builtin(v, f->sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v) {
  /* Evaluate Sexpressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  /* All other lval types remain the same */
  return v;
}


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

int main(int argc, char** argv) {
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Integer  = mpc_new("integer");
  mpc_parser_t* Decimal  = mpc_new("decimal");
  mpc_parser_t* OpSymbol = mpc_new("opsymbol");
  mpc_parser_t* OpWord   = mpc_new("opword");
  mpc_parser_t* Symbol   = mpc_new("symbol");
  mpc_parser_t* Sexpr    = mpc_new("sexpr");
  mpc_parser_t* Qexpr    = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
	    "                                                           \
    decimal  : /-?[0-9]+\\.[0-9]+/ ;					\
    integer  : /-?[0-9]+/ ;						\
    number   : <decimal> | <integer> ;					\
    opsymbol : '+' | '-' | '*' | '/' | '%' ;				\
    opword   : \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" ;	\
    symbol   : <opsymbol> | <opword> ;					\
    sexpr    : '(' <expr>* ')' ;					\
    qexpr    : '{' <expr>* '}' ;					\
    expr     : <number> | <symbol> | <sexpr> | <qexpr>  ;		\
    lispy    : /^/ <expr>+ /$/ ;					\
  ",
	    Decimal, Integer, Number, OpSymbol, OpWord, Symbol, Sexpr, Qexpr, Expr, Lispy);


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

      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);

      //printf("Leaves: %i\n", count_leaves(r.output));
      //printf("Branches: %i\n", count_branches(r.output));

      mpc_ast_delete(r.output);

    } else {
      /* Otherwise Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Free parsers */
  mpc_cleanup(10, Number, Integer, Decimal, OpSymbol, OpWord, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
