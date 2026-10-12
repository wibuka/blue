#pragma once

typedef struct Ast Ast;
LLVMModuleRef llvm_generate(Ast* root, CLI cli);
