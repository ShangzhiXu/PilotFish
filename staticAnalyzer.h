//
// Created by mxu49 on 2024/9/16.
//

#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>
#include <regex>
#include <algorithm>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <iomanip>
#include <cstring>  // For strstr, strlen, etc.
#include <sstream>
#include <fstream>
#include <stack>
#include <array>
#include <memory>
#include <cstdlib>
#include <tuple>
#include <map>
#include <set>
#include "node.h"
using namespace std;
#ifndef STATICANALYZER_H
#define STATICANALYZER_H

typedef enum {
    OTHER_TYPE = 0,              // Default type for unspecified or unrecognized elements
    CONTROL_STATEMENT,           // Represents control flow statements like 'if', 'switch'
    LOOP_STATEMENT,              // Represents loop statements like 'for', 'while'
    CLASS_DECLARATION,           // Represents class declarations
    FUNCTION_DEFINITION,         // Represents function definitions
    VARIABLE_DECLARATION,        // Represents variable declarations
    FUNCTION_CALL,               // Represents function calls
    EXPRESSION,                  // Represents expressions
    TRY_CATCH,                   // Represents try-catch blocks
    CONDITIONAL_EXPRESSION,      // Represents conditional (ternary) expressions
    STRUCT_DECLARATION,          // Represents structure declarations
    UNION_DECLARATION,           // Represents union declarations
    ENUM_DECLARATION,            // Represents enumeration declarations
    COMMENT,                     // Represents comments
    MACRO,                       // Represents macros and preprocessor directives
    TEMPLATE_DECLARATION,        // Represents template declarations for classes or functions
    NAMESPACE_DEFINITION,        // Represents namespace definitions
    ANNOTATION,                  // Represents annotations (common in Java or similar languages)
    CONSTRUCTOR_DEFINITION,      // Represents constructor definitions in OOP languages
    DESTRUCTOR_DEFINITION        // Represents destructor definitions in OOP languages
} ClassificationType;



struct CodeElement {
    std::string type;
    std::string content;
    std::string functionName;
};
typedef std::map<std::string, std::pair<std::set<std::string>, std::set<std::string>> > TaintMap;
typedef std::vector<std::pair<std::string, bool>> functionInfo;
typedef std::vector<std::string> variableInfo;
class StaticAnalyzer {

private:
        std::pair<variableInfo, functionInfo> extractVariables(const std::string& expression, const std::string& type);
        std::vector<std::string> extractVariablesFromXML(const std::string& xml);
        std::vector<std::pair<std::string, bool>> extractFunctionFromXML(const std::string& xml);
        std::vector<std::string> extractFromDeclsAndExprs(const std::string& expression);
        std::vector<std::string> extractFromCall(const std::string& expression);
        xmlNode* findChildByName(xmlNode* node, const char* name);
        bool isIgnoredElement(xmlNode* node);
        bool isValidExpression(char* content, const xmlChar* nodeName);
        bool isdigit(const std::string& str);

public:
        void printTaintMap(const TaintMap& taintMap);
        ClassificationType classifyElement(xmlNode *node);
        std::vector<CodeElement> parseElements(xmlNode *node, const std::string& currentFunction = "");
        void printElements(const std::vector<CodeElement>& elements);
        std::string exec(const char* cmd);
        std::tuple<std::string, int, int> getFunctionInfo(const std::string& binary, const std::string& function_name);
        std::string preprocessCode(const std::string& code);
        void TaintAnalysis(const std::vector<CodeElement>& elements,
                           TaintMap& taintMap,
                            std::stack<std::pair<std::string, bool>>& functionStack,
                            std::map<std::string, std::vector<std::string>> definitions,
                           std::vector<Node*>& nodesInGraph,
                           std::string currentFunction,
                           std::string previousFunction,
                           bool isForward,
                           bool forceTrack = false);
        void backwardTaintAnalysis(const std::vector<CodeElement>& stmts,
                                   TaintMap& taintMap,
                                   std::stack<std::pair<std::string, bool>>& functionStack,
                                    std::map<std::string, std::vector<std::string>> definitions,
                                   std::vector<Node*>& nodesInGraph,
                                   std::string currentFunction,
                                   std::string previousFunction,
                                   bool isForward,
                                    std::set<std::string>& taintedVariables,
                                    std::set<std::string>& taintedVariablesPrev,
                                   bool forceTrack = false,
                                   bool startToTrack = false);
        void forwardTaintAnalysis(const std::vector<CodeElement>& stmts,
                                  TaintMap& taintMap,
                                  std::stack<std::pair<std::string, bool>>& functionStack,
                                    std::map<std::string, std::vector<std::string>> definitions,
                                  std::vector<Node*>& nodesInGraph,
                                  std::string currentFunction,
                                  std::string previousFunction,
                                  bool isForward,
                                    std::set<std::string>& taintedVariables,
                                    std::set<std::string>& taintedVariablesPrev,
                                  bool forceTrack = false,
                                  bool startToTrack = false);
};


#endif
