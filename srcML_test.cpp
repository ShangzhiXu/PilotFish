#include <iostream>
#include <vector>
#include <string>
#include <cctype>

class StaticAnalyzer {
public:
    static std::vector<std::string> extractFromDeclsAndExprs(const std::string& expression) {
        std::vector<std::string> vars;
        std::string buffer;
        bool capture = false;

        for (char c : expression) {
            if (std::isalnum(c) || c == '_') {
                buffer += c;
                capture = true;
            } else if (capture && (c == ' ' || c == ',' || c == ';' || c == '=' ||
                                   c == '+' || c == '-' || c == '*' || c == '/')) {
                if (!buffer.empty()) {
                    vars.push_back(buffer);
                    buffer.clear();
                }
                capture = false;
            } else {
                capture = false;
                buffer.clear();
            }
        }

        if (!buffer.empty()) vars.push_back(buffer);
        return vars;
    }
};

int main() {
    // Input string expression
    std::string expression;
    std::cout << "Enter a C++ expression: ";
    std::getline(std::cin, expression);

    // Extract variables using the StaticAnalyzer
    std::vector<std::string> vars = StaticAnalyzer::extractFromDeclsAndExprs(expression);

    // Print the extracted variables
    std::cout << "Extracted variables:" << std::endl;
    for (const std::string& var : vars) {
        std::cout << var << std::endl;
    }

    return 0;
}

