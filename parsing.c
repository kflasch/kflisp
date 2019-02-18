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
     operator : '+' | '-' | '*' | '/' ;                 \
     expr     : <number> | '(' <operator> <expr>+ ')' ; \
     lispy    : /^/ <operator> <expr>+ /$/ ;            \
    ",
    Number, Operator, Expr, Lispy);
  
  puts("kflisp version 0.0.1");
  puts("Press ctrl+c to exit\n");

  while (1) {

    char* input = readline("kflisp> ");
    add_history(input);

    // parse input
    mpc_result_t r;
    if (mpc_parse("<stdin", input, Lispy, &r)) {
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    //    printf("echo %s\n", input);
    free(input);
  }

  // undefine & delete parsers
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}
