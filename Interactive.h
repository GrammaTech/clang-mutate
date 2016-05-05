#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#include "Ast.h"

#include <iostream>

namespace clang_mutate {  

void runInteractiveSession(std::istream & input);

extern std::map<std::string, bool> interactive_flags;

}

#endif
