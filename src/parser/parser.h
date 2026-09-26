#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include "command.h"

Command parse(const std::vector<std::string>& tokens);

#endif