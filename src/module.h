#pragma once

int load_module(Ast* root, const char* source_path);
char** get_module_links(void);
