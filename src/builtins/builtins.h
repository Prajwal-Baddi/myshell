#ifndef BUILTINS_H  
#define BUILTINS_H
#include "../parser/command.h"

bool isBuiltin(const Command& command);
bool executeBuiltin(const Command& command);

#endif 