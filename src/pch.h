#pragma once
#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>

#include "c_vector.h"
#include "c_string.h"
#include "c_memory.h"
#include "c_os.h"
#include "c_bit.h"
#include "c_toml.h"
#include "c_json.h"

#include "abi.h"
#include "version.h"
#include "cli.h"
#include "tokenizer.h"
#include "llvm-generator.h"
#include "type.h"
#include "module.h"
#include "ast.h"
#include "parser.h"
#include "log.h"

#include "llvm-c/Core.h"
