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
#define LASSERT(args, cond, fmt, ...) if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }
#define LASSERT_ARGNUM(args, argnum, funcname) if (args->count != argnum) { lval* err = lval_err("Function '%s' passed incorrect number of arguments. Got %i, expected %i", funcname, args->count, argnum); lval_del(args); return err; }
#define LASSERT_NONEMPTY(args, argnum, funcname) if (args->cell[argnum]->count == 0) { lval* err = lval_err("Function '%s' passed empty Q-Expression for argument %i", funcname, argnum); lval_del(args); return err; }
#define LASSERT_TYPE(args, argnum, exptype, funcname) if (args->cell[argnum]->type != exptype) { lval* err = lval_err("Function '%s' passed incorrect type for argument %i. Got %s, expected %s", funcname, argnum, ltype_name(args->cell[argnum]->type), ltype_name(exptype)); lval_del(args); return err; }

/* Setup */
enum { LVAL_INT, LVAL_DEC, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

char* ltype_name(int t) {
  switch(t) {
  case LVAL_FUN: return "Function";
  case LVAL_INT: return "Integer";
  case LVAL_DEC: return "Decimal";
  case LVAL_ERR: return "Error";
  case LVAL_SYM: return "Symbol";
  case LVAL_SEXPR: return "S-Expression";
  case LVAL_QEXPR: return "Q-Expression";
  default: return "Unknown";
  }
}

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
  int type;
  double num;

  char* err;
  char* sym;
  lbuiltin fun;
  
  int count;
  struct lval** cell;
};

struct lenv {
  int count;
  char** syms;
  lval** vals;

  int count_reserved;
  char** reserved;
};

/* lval Helper functions */

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
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = malloc(512);

  vsnprintf(v->err, 511, fmt, va);

  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);

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

lval* lval_fun(char* name, int argcount, lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  v->sym = name;
  v->count = argcount;
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

  case LVAL_FUN: break;

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

lval* lval_copy(lval* v) {  
  lval* x = malloc(sizeof(lval));
  x->type = v->type;
  
  switch (v->type) {
    
    /* Copy Functions and Numbers Directly */
  case LVAL_FUN: x->fun = v->fun; x->sym = v->sym; x->count = v->count; break;
  case LVAL_INT: 
  case LVAL_DEC:
    x->num = v->num; break;
    
    /* Copy Strings using malloc and strcpy */
  case LVAL_ERR: x->err = malloc(strlen(v->err) + 1); strcpy(x->err, v->err); break;
  case LVAL_SYM: x->sym = malloc(strlen(v->sym) + 1); strcpy(x->sym, v->sym); break;

    /* Copy Lists by copying each sub-expression */
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval*) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  
  return x;
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
  case LVAL_FUN:   printf("<function '%s'>", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/* EVALUATION */
lval* lval_eval(lenv* e, lval* v);
lval* lval_join(lval* x, lval* y);

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

/* lenv Helper functions */
lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;

  e->count_reserved = 0;
  e->reserved = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);

  for (int i = 0; i < e->count_reserved; i++) {
    free(e->reserved[i]);
  }
  free(e->reserved);

  free(e);  
}

lval* lenv_get(lenv* e, lval* k) {

  /* Iterate over all items in environment */
  for (int i = 0; i < e->count; i++) {
    /* Check if the stored string matches the symbol string */
    /* If it does, return a copy of the value */
    if (strcmp(e->syms[i], k->sym) == 0) { return lval_copy(e->vals[i]); }
  }
  /* If no symbol found return error */
  return lval_err("Unbound symbol '%s'", k->sym);
}

int lenv_sym_reserved(lenv* e, lval* s) {
  for (int i = 0; i < e->count_reserved; i++) {
    if (strcmp(e->reserved[i], s->sym) == 0) {
      return 1;
    }
  }
  return 0;
}

void lenv_put(lenv* e, lval* k, lval* v) {
  
  /* Iterate over all items in environment */
  /* This is to see if variable already exists */
  for (int i = 0; i < e->count; i++) {

    /* If variable is found delete item at that position */
    /* And replace with variable supplied by user */
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* If no existing entry found then allocate space for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* Copy contents of lval and symbol string into new location */
  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

void lenv_put_reserved(lenv* e, lval* k, lval* v) { 
  lenv_put(e, k, v);
  
  if (!lenv_sym_reserved(e, k)) {
    e->count_reserved++;
    e->reserved = realloc(e->reserved, sizeof(char*) * e->count_reserved);
    e->reserved[e->count_reserved - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->reserved[e->count_reserved - 1], k->sym);
  }
}

/* Builtin functions */

lval* builtin_head(lenv* e, lval* a) {
  /* Check Error Conditions */
  LASSERT_ARGNUM(a, 1, "head");
  LASSERT_TYPE(a, 0, LVAL_QEXPR, "head");
  LASSERT_NONEMPTY(a, 0, "head");

  /* Otherwise take first argument */
  lval* v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  /* Check Error Conditions */
  LASSERT_ARGNUM(a, 1, "tail");
  LASSERT_TYPE(a, 0, LVAL_QEXPR, "tail");
  LASSERT_NONEMPTY(a, 0, "tail");

  /* Take first argument */
  lval* v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_cons(lenv* e, lval* a) {
  LASSERT_ARGNUM(a, 2, "cons");
  LASSERT_TYPE(a, 1, LVAL_QEXPR, "cons");
  LASSERT_NONEMPTY(a, 1, "cons");

  lval* result = lval_qexpr();
  lval_add(result, lval_pop(a, 0));

  return lval_join(result, lval_take(a, 0));
}

lval* builtin_len(lenv* e, lval* a) {
  LASSERT_ARGNUM(a, 1, "len");
  LASSERT_TYPE(a, 0, LVAL_QEXPR, "len");

  return lval_num(a->cell[0]->count);
}

lval* builtin_init(lenv* e, lval* a) {
  LASSERT_ARGNUM(a, 1, "init");
  LASSERT_TYPE(a, 0, LVAL_QEXPR, "init");
  LASSERT_NONEMPTY(a, 0, "init");
  
  lval_del(lval_pop(a->cell[0], a->cell[0]->count - 1));

  return a->cell[0];
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT_ARGNUM(a, 1, "eval");
  LASSERT_TYPE(a, 0, LVAL_QEXPR, "eval");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
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

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE(a, i, LVAL_QEXPR, "join");
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
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

lval* builtin_def(lenv* e, lval* a) {
  LASSERT_TYPE(a, 0, LVAL_QEXPR, "def");

  /* First argument is symbol list */
  lval* syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM), "Function 'def' cannot define non-symbol");
    LASSERT(a, (!lenv_sym_reserved(e, syms->cell[i])), "Function 'def' cannot redefine reserved symbol");
  }
  
  /* Check correct number of symbols and values */
  LASSERT(a, (syms->count == a->count-1), "Function 'def' cannot define incorrect number of values to symbols");

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }

  lval_del(a);
  return lval_sexpr();
}

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }
lval* builtin_mod(lenv* e, lval* a) { return builtin_op(e, a, "%"); }

lval* builtin_vars(lenv* e, lval* a) {
  for (int i = 0; i < e->count; i++) {
    printf("%s\n", e->syms[i]);
  }
  return a;
}

lval* builtin_reserved(lenv* e, lval* a) {
  for (int i = 0; i < e->count_reserved; i++) {
    printf("%s\n", e->reserved[i]);
  }
  return a;
}

lval* builtin_exit(lenv* e, lval* a) {
  printf("Exiting\n");
  return a;
}

void lenv_add_builtin(lenv* e, char* name, int argcount, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(name, argcount, func);
  lenv_put_reserved(e, k, v);
  lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {  
  /* List Functions */
  lenv_add_builtin(e, "list", -1, builtin_list); lenv_add_builtin(e, "cons", -1, builtin_cons);
  lenv_add_builtin(e, "head",  1, builtin_head); lenv_add_builtin(e, "tail",  1, builtin_tail);
  lenv_add_builtin(e, "eval", -1, builtin_eval); lenv_add_builtin(e, "join", -1, builtin_join);
  lenv_add_builtin(e, "len",  -1, builtin_len);  lenv_add_builtin(e, "init", -1, builtin_init);

  /* Mathematical Functions */
  lenv_add_builtin(e, "+", -1, builtin_add); lenv_add_builtin(e, "-", -1, builtin_sub);
  lenv_add_builtin(e, "*", -1, builtin_mul); lenv_add_builtin(e, "/", -1, builtin_div);
  lenv_add_builtin(e, "%", -1, builtin_mod);

  lenv_add_builtin(e, "def",     -1, builtin_def);
  lenv_add_builtin(e, "vars",     0, builtin_vars);
  lenv_add_builtin(e, "reserved", 0, builtin_reserved);
  lenv_add_builtin(e, "exit",     0, builtin_exit);
}

lval* lval_eval_sexpr(lenv* e, lval* v) {

  /* Evaluate Children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* Error Checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* Empty Expression */
  if (v->count == 0) { return v; }

  /* Single Expression */
  if (v->count == 1) { return lval_take(v, 0); }

  /* Ensure First Element is a function after evaluation */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f); lval_del(v);
    return lval_err("First element is not a function!");
  }

  /* Call builtin with operator */
  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  /* Evaluate symbols using environment */
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);

    /* Evaluate functions taking no arguments*/
    if ((x->type == LVAL_FUN) && (x->count == 0)) {
	x->fun(e,x);
    }

    lval_del(v);

    return x;
  }

  /* Evaluate Sexpressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }

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
    symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ;			\
    sexpr    : '(' <expr>* ')' ;					\
    qexpr    : '{' <expr>* '}' ;					\
    expr     : <number> | <symbol> | <sexpr> | <qexpr>  ;		\
    lispy    : /^/ <expr>+ /$/ ;					\
  ",
	    Decimal, Integer, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);


  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.1");
  puts("Type exit or press Ctrl+c to Exit\n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  /* In a never ending loop */
  int exit = 0;
  while (!exit) {

    /* Output our prompt and get input*/
    char* input = readline("lispy> ");

    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On Success Print the AST */
      
      //mpc_ast_print(r.output);

      lval* x = lval_eval(e, lval_read(r.output));

      if (strcmp(x->sym, "exit") == 0) 
	exit = 1;

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

  lenv_del(e);

  /* Free parsers */
  mpc_cleanup(8, Number, Integer, Decimal, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
