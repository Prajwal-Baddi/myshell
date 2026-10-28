#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include "command.h"

Pipeline parse(const std::vector<std::string>& tokens);
CommandSequence parseSequence(const std::vector<std::string>& tokens);

#endif
