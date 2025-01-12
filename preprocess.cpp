#include <iostream>
#include <string>
#include <regex>
#include <vector>

// Function to remove 'sizeof' operators and type casts while retaining their inner content
std::string removeSizeofAndCasts(const std::string& code) {
    std::string modifiedCode = code;

    // Regex to match reinterpret_cast or static_cast or dynamic_cast or const_cast
    // Pattern breakdown:
    // (reinterpret_cast|static_cast|dynamic_cast|const_cast)      : Match the cast type
    // \s*<\s*([^>]+)\s*>                                        : Match the template parameter inside <>
    // \s*\(\s*([^()]+)\s*\)                                     : Match the expression inside ()
    std::regex cast_regex(R"((reinterpret_cast|static_cast|dynamic_cast|const_cast)\s*<\s*([^>]+)\s*>\s*\(\s*([^()]+)\s*\))");

    // Replace the entire cast expression with just the inner expression (third capture group)
    modifiedCode = std::regex_replace(code, cast_regex, "$3");

    // 2. Remove C-style casts by replacing them with their inner expressions
    // Pattern: (type) expression
    // Replacement: expression
    // This pattern handles types with pointers, references, namespaces, and templates
    std::regex c_cast_regex(R"(\(\s*[a-zA-Z_:][a-zA-Z0-9_:<>\*\&\s]*\s*\)\s*([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    modifiedCode = std::regex_replace(modifiedCode, c_cast_regex, "$1");

    // 3. Replace sizeof(expression) with expression
    // Pattern: sizeof ( expression )
    // Replacement: expression
    std::regex sizeof_with_parentheses_regex(R"(sizeof\s*\(\s*([^()]+?)\s*\))");
    modifiedCode = std::regex_replace(modifiedCode, sizeof_with_parentheses_regex, "$1");

    // 4. Replace sizeof type with type
    // Pattern: sizeof type
    // Replacement: type
    // This pattern handles types with namespaces and template parameters
    std::regex sizeof_without_parentheses_regex(R"(sizeof\s+([a-zA-Z_:][a-zA-Z0-9_:<>]*))");
    modifiedCode = std::regex_replace(modifiedCode, sizeof_without_parentheses_regex, "$1");

    return modifiedCode;
}

int main() {
    // Test Cases: Pair of Original and Expected Modified Statements
    std::vector<std::pair<std::string, std::string>> testCases = {
            // 1. C-style cast and sizeof with parentheses
            {
                    "int var = (int)memcpy(&smallBuffer, &largeBuffer, sizeof(int));",
                    "int var = memcpy(&smallBuffer, &largeBuffer, int);"
            },
            // 2. C-style cast and sizeof without parentheses
            {
                    "int size = sizeof int;",
                    "int size = int;"
            },
            // 3. C++-style cast and sizeof with expression
            {
                    "double total = static_cast<double>(sizeof(a + b));",
                    "double total = a + b;"
            },
            // 4. C++-style cast with template type and sizeof
            {
                    "auto len = reinterpret_cast<std::vector<int>*>(malloc(sizeof(std::vector<int>)));",
                    "auto len = malloc(std::vector<int>);"
            },
            // 5. Multiple casts and sizeof
            {
                    "float f = (float)(sizeof(char) + static_cast<float>(sizeof(double)));",
                    "float f = char + float(double);"
            },
            // 6. Nested sizeof
            {
                    "int size = sizeof(sizeof(int));",
                    "int size = sizeof(int);"
            },
            // 7. sizeof with pointers and references
            {
                    "void* p = sizeof(void*);",
                    "void* p = void*;"
            },
            // 8. C++-style const_cast and sizeof
            {
                    "auto ptr = const_cast<char*>(malloc(sizeof(char) * 10));",
                    "auto ptr = malloc(char * 10);"
            },
            // 9. C-style cast without spaces and sizeof with spaces
            {
                    "int* ptr = (int*)malloc( sizeof(int) * 5 );",
                    "int* ptr = malloc( int * 5 );"
            },
            // 10. sizeof in function call without cast
            {
                    "size_t len = sizeof(myStruct);",
                    "size_t len = myStruct;"
            },
            // 11. C++-style cast with multiple template parameters
            {
                    "auto ptr = dynamic_cast<std::map<std::string, int>*>(getMap());",
                    "auto ptr = getMap();"
            },
            // 12. C-style cast with pointers
            {
                    "double* dp = (double*)malloc(sizeof(double) * 10);",
                    "double* dp = malloc(double * 10);"
            },
            // 13. C++-style cast without spaces
            {
                    "int x = static_cast<int>(y);",
                    "int x = y;"
            },
            // 14. Multiple sizeof and casts in one line
            {
                    "auto buffer = (char*)malloc(sizeof(char) * sizeof(int));",
                    "auto buffer = malloc(char * int);"
            },
            // 15. sizeof with complex expressions
            {
                    "int size = sizeof(a + b * c);",
                    "int size = a + b * c;"
            }
    };

    // Process each test case
    for (const auto& testCase : testCases) {
        const std::string& original = testCase.first;
        const std::string& expected = testCase.second;

        std::string modified = removeSizeofAndCasts(original);

        // Display results
        std::cout << "Original: " << original << "\n";
        std::cout << "Modified: " << modified << "\n";
        std::cout << "Expected: " << expected << "\n";
        std::cout << (modified == expected ? "PASS" : "FAIL") << "\n\n";
    }

    return 0;
}
