#pragma once

int link_object(CLI cli, const char* opt_flag, const char* obj_file);
int compile(CLI cli, LLVMModuleRef module);
