#include <iostream>
#include "tokenizer.h"

int main() {
    std::string input = "ls -l /home";
    std::string quoted = "echo \"Hello world\" 'single quoted'";

    for (const auto& token : tokenize(input)) {
        std::cout << "[" << token << "]\n";
    }
    std::cout << "---\n";
    for (const auto& token : tokenize(quoted)) {
        std::cout << "[" << token << "]\n";
    }
}