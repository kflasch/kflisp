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
enum { LVAL_NUM, LVAL_ERR };

typedef struct {
  int type;
  long num;
  int err;
} lval;

// creates a new number type lval
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

// creates a new error type lval
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void print_lval(lval v) {
  switch (v.type) {
  case LVAL_NUM:
    printf("%li", v.num);
    break;
  case LVAL_ERR:
    if (v.err == LERR_DIV_ZERO) {
      printf("Error: Division/Modulo By Zero!");
    } else if (v.err == LERR_BAD_OP) {
      printf("Error: Invalid Operator!");
    } else if (v.err == LERR_BAD_NUM) {
      printf("Error: Invalid Number!");
    }
    break;
  }
}

void println_lval(lval v) { print_lval(v); putchar('\n'); }

// evaluate operator string to find operation to perform
lval eval_op(lval x, char* op, lval y) {

  // check for errors
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }
  
  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }
  if (strcmp(op, "%") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num % y.num);
  }
  
  //  if (strcmp(op, "^") == 0) { return pow(x, y); } // bad because expects floating points?
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {

  // if tagged as number return it directly
  // check for error in conversion
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  // the operator is always the second child
  char* op = t->children[1]->contents;
  // store the third child in 'x'
  lval x = eval(t->children[2]);

  //for (int i=0; i<t->children_num; i++)
  //  printf("%s\n", t->children[i]->tag);  
  //printf("size: %i\n\n", t->children_num);

  // handle negating single ints (e.g., - 2)
  if (!strstr(t->children[3]->tag, "expr")) {
    if (strcmp(op, "-") == 0) { return lval_num(-x.num); }
  }
  
  // iterate on remaining children
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char** argv) {

  // create parsers
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  // define parsers
  mpca_lang(MPCA_LANG_DEFAULT,
    " \
     number   : /-?[0-9]+/ ;                            \
     operator : '+' | '-' | '*' | '/' | '%' | '^' ;           \
     expr     : <number> | '(' <operator> <expr>+ ')' ; \
     lispy    : /^/ <operator> <expr>+ /$/ ;            \
    ",
    Number, Operator, Expr, Lispy);
  
  puts("kflisp version 0.0.3");
  puts("Press ctrl+c to exit\n");

  while (1) {

    char* input = readline("kflisp> ");
    add_history(input);

    // parse input
    mpc_result_t r;
    if (mpc_parse("<stdin", input, Lispy, &r)) {
      lval result = eval(r.output);
      println_lval(result);
      //printf("%li\n", result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }

  // undefine & delete parsers
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}