#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "../parser/command.h"

int executeCommand(const Command& command);
int executePipeline(const Pipeline& pipeline);
#endif