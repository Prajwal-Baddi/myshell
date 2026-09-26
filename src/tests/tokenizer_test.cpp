#include <iostream>
#include "tokenizer.h"

int main() {
    std::string input = "ls -l /home";

    std::vector<std::string> tokens = tokenize(input);

    for (const auto& token : tokens) {
        std::cout << "[" << token << "]\n";
    }
}