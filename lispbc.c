#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

// fake readline
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

// fake add_history
void add_history(char* unused) {}

#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

// error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// lval types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  // count of and pointer to list of lval*
  int count;
  struct lval** cell;
} lval;

// creates a pointer to a new Number lval
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

// creates a pointer to a new Error lval
lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

// creates a pointer to a new Symbol lval
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

// creates a pointer to a new S-expression lval
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
  case LVAL_NUM: break;
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;
  case LVAL_SEXPR:
    // delete all elements inside S-expression
    for (int i=0; i<v->count; i++) {
      lval_del(v->cell[i]);
    }
    // free memory allocated for pointers
    free(v->cell);
    break;
  }

  // free lval struct memory
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
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {

  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  // if root ('>') or s-expression, create empty list
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

  for (int i=0; i<t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

void print_lval(lval* v);

void print_lval_expr(lval* v, char open, char close) {
  putchar(open);
  for (int i=0; i<v->count; i++) {
    print_lval(v->cell[i]);
    if (i != (v->count-1))
      putchar(' ');
  }
  putchar(close);
}

void print_lval(lval* v) {
  switch (v->type) {
  case LVAL_NUM:    
    printf("%li", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    print_lval_expr(v, '(', ')');
    break;
  }
}

void println_lval(lval* v) { print_lval(v); putchar('\n'); }

// gets element from s-expr at i and shifts list backward to remove it
lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  // shift memory after item at i
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  // decrease count of items in list
  v->count--;

  // reallocate memory used
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

// pops element i off of v and also deletes v
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* a, char* op) {

  for (int i=0; i<a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number");
    }
  }

  lval* x = lval_pop(a, 0);

  // negate number if op is - and no further elements
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while (a->count > 0) {
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Modulo by zero!");
        break;
      }
      x->num %= y->num;
    }
    if (strcmp(op, "^") == 0) {
      x->num = pow(x->num, y->num);
    } // bad because expects floating points?

    lval_del(y);
  }

  lval_del(a);
  return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {

  // eval children
  for (int i=0; i<v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  // error checking
  for (int i=0; i<v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  // empty expr
  if (v->count == 0) { return v; }

  // single expr
  if (v->count == 1) { return lval_take(v, 0); }

  // ensure first element is symbol
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with symbol!");
  }

  lval* result = builtin_op(v, f->sym);
  lval_del(f);
  return result;
}

// eval s-expressions if needed
lval* lval_eval(lval* v) {
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
}

int main(int argc, char** argv) {

  // create parsers
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispbc = mpc_new("lispbc");

  // define parsers
  mpca_lang(MPCA_LANG_DEFAULT,
    " \
     number   : /-?[0-9]+/ ;                            \
     symbol   : '+' | '-' | '*' | '/' | '%' | '^' ;     \
     sexpr    : '(' <expr>* ')' ;                       \
     expr     : <number> | <symbol> | <sexpr> ;         \
     lispbc   : /^/ <expr>* /$/ ;                       \
    ",
    Number, Symbol, Sexpr, Expr, Lispbc);
  
  puts("lispbc version 0.0.7");
  puts("Press ctrl+c to exit\n");

  while (1) {

    char* input = readline("lispbc> ");
    add_history(input);

    // parse input
    mpc_result_t r;
    if (mpc_parse("<stdin", input, Lispbc, &r)) {
      lval* x = lval_eval(lval_read(r.output));
      println_lval(x);
      lval_del(x);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }

  // undefine & delete parsers
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispbc);

  return 0;
}
