#pragma once

typedef struct Ast Ast;
typedef struct CLI CLI;

void ast_explorer(Ast *root);
int  ast_dump(Ast *root, CLI *cli);
