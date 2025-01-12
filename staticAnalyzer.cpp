//
// Created by mxu49 on 2024/9/16.
//

#include "staticAnalyzer.h"

xmlNode* StaticAnalyzer::findChildByName(xmlNode* node, const char* name) {
    for (xmlNode* child = node->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && !xmlStrcmp(child->name, (const xmlChar*)name)) {
            return child;
        }
    }
    return nullptr;
}




bool StaticAnalyzer::isdigit(const std::string& str) {
    return std::all_of(str.begin(), str.end(), ::isdigit);
}

ClassificationType StaticAnalyzer::classifyElement(xmlNode *node) {
    if (node == NULL)
        return OTHER_TYPE;

    const char* elementName = (const char*)node->name;

    // Control structures
    if (!xmlStrcmp(node->name, (const xmlChar *)"if")) {
        return CONTROL_STATEMENT;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"for") || !xmlStrcmp(node->name, (const xmlChar *)"while")) {
        return LOOP_STATEMENT;
    }

        // Data structures and types
    else if (!xmlStrcmp(node->name, (const xmlChar *)"class")) {
        return CLASS_DECLARATION;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"struct")) {
        return STRUCT_DECLARATION;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"union")) {
        return UNION_DECLARATION;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"enum")) {
        return ENUM_DECLARATION;

        // Functions and methods
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"function")) {
        return FUNCTION_DEFINITION;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"constructor")) {
        return CONSTRUCTOR_DEFINITION;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"destructor")) {
        return DESTRUCTOR_DEFINITION;

        // Comments and annotations
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"comment")) {
        return COMMENT;

        // Other possible elements
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"template")) {
        return TEMPLATE_DECLARATION;
    } else if (!xmlStrcmp(node->name, (const xmlChar *)"namespace")) {
        return NAMESPACE_DEFINITION;

        // Fallback for other types
    }

    return OTHER_TYPE;
}


std::vector<CodeElement> StaticAnalyzer::parseElements(xmlNode *node, const std::string& currentFunction) {
    std::vector<CodeElement> elements;

    if (node == NULL) return elements;

    std::string functionContext = currentFunction;
    if (node->type == XML_ELEMENT_NODE && !xmlStrcmp(node->name, (const xmlChar *)"function")) {
        // Assuming the function name is directly under a <name> child of <function>
        xmlNode* nameNode = findChildByName(node, "name");
        if (nameNode) {
            xmlChar* functionName = xmlNodeGetContent(nameNode);
            if (functionName) {
                functionContext = (char*)functionName;
                xmlFree(functionName);
            }
        }
    }

    // Recursively process all children to find relevant code elements
    for (xmlNode *child = node->children; child; child = child->next) {
        if (isIgnoredElement(child)) continue;
        auto childElements = parseElements(child, functionContext);
        elements.insert(elements.end(), childElements.begin(), childElements.end());
    }

    // Process the current node if it's of an valid type
    if (node->type == XML_ELEMENT_NODE) {
        xmlChar* content = xmlNodeGetContent(node);
        if (isValidExpression((char*)content, node->name)) {
            // for each valid expression, we need to copy the node
            // and then remove the child node of type "type"
            // and then update the content
            xmlNode* typeNode = findChildByName(node, "type");
            if (typeNode) {
                xmlUnlinkNode(typeNode);
                xmlFreeNode(typeNode);
            }
            content = xmlNodeGetContent(node);


            elements.push_back({std::string((char*)node->name), std::string((char*)content), functionContext});
        }
        xmlFree(content);
    }

    return elements;
}


void StaticAnalyzer::printElements(const std::vector<CodeElement>& elements) {
    for (const auto& elem : elements) {
        std::cout << "Function: " << elem.functionName << ", Type: " << elem.type << ", Content: " << elem.content << std::endl;
    }
}



bool StaticAnalyzer::isIgnoredElement(xmlNode* node) {
    return (!xmlStrcmp(node->name, (const xmlChar *)"comment") ||
            !xmlStrcmp(node->name, (const xmlChar *)"function_decl") ||
            !xmlStrcmp(node->name, (const xmlChar *)"type") );
}


bool StaticAnalyzer::isValidExpression(char* content, const xmlChar* nodeName) {
    // Simplified checks for assignment or comparison within the expression
    return ( !xmlStrcmp(nodeName, (const xmlChar *)"decl") ||
            (!xmlStrcmp(nodeName, (const xmlChar *)"parameter")) ||
             (!xmlStrcmp(nodeName, (const xmlChar *)"return")) ||
             (!xmlStrcmp(nodeName, (const xmlChar *)"call")) ||
             (!xmlStrcmp(nodeName, (const xmlChar *)"expr") && (strstr(content, "=") || strstr(content, "==") ||
                                                                strstr(content, "<") || strstr(content, ">") ||
                                                                strstr(content, "+") || strstr(content, "-") ||
                                                                strstr(content, "*") || strstr(content, "/") )));
}

std::vector<std::string> StaticAnalyzer::extractVariablesFromXML(const std::string& xml){

    std::vector<std::string> variables;

    // 初始化 libxml
    xmlInitParser();

    // 从内存中解析 XML
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.length(), "noname.xml", NULL, XML_PARSE_NOBLANKS);
    if (doc == NULL) {
        std::cerr << "Error: unable to parse XML." << std::endl;
        return variables;
    }

    // 创建 XPath 上下文
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        std::cerr << "Error: unable to create new XPath context." << std::endl;
        xmlFreeDoc(doc);
        return variables;
    }

    // 注册 'src' 命名空间
    if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "src", BAD_CAST "http://www.srcML.org/srcML/src") != 0) {
        std::cerr << "Error: unable to register namespace." << std::endl;
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return variables;
    }

    // 调整后的 XPath 表达式：
    // 选择所有 <src:name> 元素，这些元素：
    // - 不位于 <src:type> 的后代
    // - 不位于 <src:call> 或 <src:macro> 的子节点
    // - 不紧跟在 <src:operator> '->' 之后
    const xmlChar* xpathExpr = BAD_CAST "//src:name["
                                        "not(ancestor::src:type) and "
                                        "not(parent::src:call) and "
                                        "not(parent::src:macro) and "
                                        "not(preceding-sibling::src:operator[text()='-&gt;']) and "
                                        "not(child::src:index)"
                                        "]";

    xmlXPathObjectPtr nameNodesResult = xmlXPathEvalExpression(xpathExpr, xpathCtx);

    if (nameNodesResult && nameNodesResult->nodesetval) {
        for (int i = 0; i < nameNodesResult->nodesetval->nodeNr; ++i) {
            xmlNodePtr node = nameNodesResult->nodesetval->nodeTab[i];

            // 提取名称内容
            xmlChar* content = xmlNodeGetContent(node);
            if (content) {
                std::string varName = reinterpret_cast<char*>(content);
                // 过滤关键词
                static const std::vector<std::string> keywords = {
                        "int", "char", "void", "NULL", "errno", "sizeof", "defined"
                };
                bool isKeyword = std::find(keywords.begin(), keywords.end(), varName) != keywords.end();
                if (!isKeyword &&
                    varName.find("TINYDIR_STRING") == std::string::npos &&
                    varName.find("_FUNC") == std::string::npos) {
                    variables.push_back(varName);
                }
                xmlFree(content);
            }
        }
    }
    xmlXPathFreeObject(nameNodesResult);

    // 清理
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    // 去重并排序
    std::sort(variables.begin(), variables.end());
    variables.erase(std::unique(variables.begin(), variables.end()), variables.end());

    return variables;

}



std::vector<std::pair<std::string, bool>> StaticAnalyzer::extractFunctionFromXML(const std::string& xml) {

    // vector of {functionname. hasArguments}
    std::vector<std::pair<std::string, bool>> functions = {};
    // Parse the XML from memory
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.length(), "noname.xml", NULL, XML_PARSE_NOBLANKS);
    if (doc == NULL) {
        std::cerr << "Error: unable to parse XML." << std::endl;
        return functions;
    }

    // Create XPath context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        std::cerr << "Error: unable to create new XPath context." << std::endl;
        xmlFreeDoc(doc);
        return functions;
    }

    // Register the 'src' namespace with the prefix 'src'
    if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "src", BAD_CAST "http://www.srcML.org/srcML/src") != 0) {
        std::cerr << "Error: unable to register namespace." << std::endl;
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return functions;
    }

    //TODO: remove //src:macro//src:name need to be verified
    const xmlChar* xpathExpr = BAD_CAST "//src:call/src:name" ;
    xmlXPathObjectPtr nameNodesResult = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if (nameNodesResult && nameNodesResult->nodesetval) {
        int size = nameNodesResult->nodesetval->nodeNr;
        for (int i = 0; i < size; ++i) {
            xmlNodePtr node = nameNodesResult->nodesetval->nodeTab[i];

            xmlChar* content = xmlNodeGetContent(node);
            if (content) {
                std::string funcName = (char*)content;
                //remove whitespace
                funcName.erase(std::remove_if(funcName.begin(), funcName.end(), ::isspace), funcName.end());

                xmlNodePtr parent = node->parent;
                bool hasArguments = false;

                if (parent) {
                    xmlNodePtr argList = parent->children;
                    while (argList) {
                        if (xmlStrcmp(argList->name, BAD_CAST "argument_list") == 0) {
                            // check whether <argument_list> have <argument> in it
                            xmlNodePtr arg = argList->children;
                            while (arg) {
                                if (xmlStrcmp(arg->name, BAD_CAST "argument") == 0) {
                                    hasArguments = true;
                                    break;
                                }
                                arg = arg->next;
                            }
                            break;
                        }
                        argList = argList->next;
                    }
                }

                if (!funcName.empty()) {
                    functions.push_back({funcName, hasArguments});
                }
                xmlFree(content);
            }
        }
    }
    if (nameNodesResult) {
        xmlXPathFreeObject(nameNodesResult);
    }

    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return functions;
}



std::vector<std::string> StaticAnalyzer::extractFromCall(const std::string& expression) {
    std::vector<std::string> vars;
    std::string buffer;
    bool inBrackets = false;

    for (size_t i = 0; i < expression.length(); ++i) {
        if (expression[i] == '(') {
            inBrackets = true;
        } else if (expression[i] == ')') {
            if (!buffer.empty()) {
                vars.push_back(buffer);
                buffer.clear();
            }
            inBrackets = false;
        } else if (inBrackets) {
            if (std::isalnum(expression[i]) || expression[i] == '_') {
                buffer += expression[i];
            } else if (expression[i] == ',') {
                // handle when const strings are optimized at preprocess, only leave blank space
                vars.push_back(buffer);
                buffer.clear();
            } else if (expression[i] == ',' || std::isspace(expression[i])) {
                if (!buffer.empty()) {
                    vars.push_back(buffer);
                    buffer.clear();
                }
            }
        }
    }

    return vars;
}


std::pair<variableInfo, functionInfo> StaticAnalyzer::extractVariables(const std::string& expression,
                                                          const std::string& type) {
    std::vector<std::string> variables = {};
    std::vector<std::pair<std::string, bool>>  functionCalls = {};
    if (type == "decl" || type == "expr") {
        std::string command = "echo \"" + expression + "\" | srcml --language=C++ --position  ";
        std::string srcml_output = exec(command.c_str());
        variables = extractVariablesFromXML(srcml_output);
        std::cout << "variables: " ;
        for (auto var : variables) {
            std::cout << var << " ";
        }
        std::cout << std::endl;
        functionCalls = extractFunctionFromXML(srcml_output);
    } else if (type == "call") {
        variables = extractFromCall(expression);
        std::string command = "echo \"" + expression + "\" | srcml --language=C++ --position  ";
        std::string srcml_output = exec(command.c_str());
        functionCalls = extractFunctionFromXML(srcml_output);
    } else if (type == "parameter") {
        std::istringstream stream(expression);
        std::string var;
        while (stream >> var) {
            variables.push_back(var);
        }
    }

    return {variables, functionCalls};
}


// Function to execute a shell command and capture its output
std::string StaticAnalyzer::exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Function to find start/end line and file name of a function
std::tuple<std::string, int, int> StaticAnalyzer::getFunctionInfo(const std::string& binary,
                                                                  const std::string& function_name) {
    ////TODO: find out how to get the start and end line of a function
    //      that is not in the target binary, but in linked libraries

    // Step 1: Use nm to get the start address and size of the function
    std::string nm_cmd = "nm -S " + binary + " | grep '[Tt]' | grep " + function_name;
    std::string nm_output = exec(nm_cmd.c_str());

    if (nm_output.empty()) {
        return {"", 0, 0};
    }

    // Step 2: Split nm_output into lines
    std::vector<std::string> lines;
    std::istringstream nm_stream(nm_output);
    std::string line;
    while (std::getline(nm_stream, line)) {
        lines.push_back(line);
    }

    std::string start_addr, size, type, name;
    bool found = false;

    for (const auto& ln : lines) {
        std::istringstream iss(ln);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 4 && (tokens[2] == "T" || tokens[2] == "t") && tokens[3] == function_name) {
            start_addr = tokens[0];
            size = tokens[1];
            type = tokens[2];
            name = tokens[3];
            found = true;
            break;
        }
    }

    if (!found) {
        return {"", 0, 0};
    }

    // Adjust start address
    unsigned long hex = std::stoul(start_addr, nullptr, 16);
    if (hex > 0) {
        hex -= 1;
    }
    std::stringstream start_addr_ss;
    start_addr_ss << std::setfill('0') << std::setw(start_addr.length()) << std::hex << hex;
    start_addr = start_addr_ss.str();

    // Convert the start address and size to decimal
    unsigned long start = std::stoul(start_addr, nullptr, 16);
    unsigned long func_size = std::stoul(size, nullptr, 16);
    unsigned long end = start + func_size;

    // Step 3: Use addr2line to find the start line and file
    std::string start_addr_cmd = "addr2line -e " + binary + " -f -C 0x" + start_addr;
    std::string start_info = exec(start_addr_cmd.c_str());
    while (start_info.find("?") != std::string::npos && hex < end) {
        hex += 1;
        start_addr_ss.str("");
        start_addr_ss << std::setfill('0') << std::setw(start_addr.length()) << std::hex << hex;
        start_addr_cmd = "addr2line -e " + binary + " -f -C 0x" + start_addr_ss.str();
        start_info = exec(start_addr_cmd.c_str());
    }

    // Step 4: Use addr2line to find the end line and file
    std::stringstream end_addr_ss;
    end_addr_ss << std::hex << end;
    std::string end_addr_cmd = "addr2line -e " + binary + " -f -C 0x" + end_addr_ss.str();
    std::string end_info = exec(end_addr_cmd.c_str());
    while (end_info.find("?") != std::string::npos && end > start) {
        end -= 1;
        end_addr_ss.str("");
        end_addr_ss << std::hex << end;
        end_addr_cmd = "addr2line -e " + binary + " -f -C 0x" + end_addr_ss.str();
        end_info = exec(end_addr_cmd.c_str());
    }

    // Step 5: Parse start_info and end_info
    std::string file;
    int start_line = 0, end_line = 0;

    size_t pos = start_info.find('\n');
    if (pos != std::string::npos) {
        file = start_info.substr(pos + 1);
        size_t colon_pos = file.find(':');
        if (colon_pos != std::string::npos) {
            file = file.substr(0, colon_pos);
        }
    }

    try {
        start_line = std::stoi(start_info.substr(start_info.find(':') + 1));
        end_line = std::stoi(end_info.substr(end_info.find(':') + 1));
    } catch (...) {
        return {"", 0, 0};
    }

    return {file, start_line, end_line};
}


std::string StaticAnalyzer::preprocessCode(const std::string& code) {
    // preprocess the output, remove comments and empty lines, as well as the const strings
    // then make them in one line
    std::string output;
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        // Regex to match reinterpret_cast or static_cast or dynamic_cast or const_cast
        // Pattern breakdown:
        // (reinterpret_cast|static_cast|dynamic_cast|const_cast)      : Match the cast type
        // \s*<\s*([^>]+)\s*>                                        : Match the template parameter inside <>
        // \s*\(\s*([^()]+)\s*\)                                     : Match the expression inside ()
        std::regex cast_regex(R"((reinterpret_cast|static_cast|dynamic_cast|const_cast)\s*<\s*([^>]+)\s*>\s*\(\s*([^()]+)\s*\))");

        // Replace the entire cast expression with just the inner expression (third capture group)
        line = std::regex_replace(line, cast_regex, "$3");

        // 2. Remove C-style casts by replacing them with their inner expressions
        // Pattern: (type) expression
        // Replacement: expression
        // This pattern handles types with pointers, references, namespaces, and templates
        std::regex c_cast_regex(R"(\(\s*[a-zA-Z_:][a-zA-Z0-9_:<>\*\&\s]*\s*\)\s*([a-zA-Z_][a-zA-Z0-9_]*)\b)");
        line = std::regex_replace(line, c_cast_regex, "$1");

        // 3. Replace sizeof(expression) with expression
        // Pattern: sizeof ( expression )
        // Replacement: expression
        std::regex sizeof_with_parentheses_regex(R"(sizeof\s*\(\s*([^()]+?)\s*\))");
        line = std::regex_replace(line, sizeof_with_parentheses_regex, "$1");

        // 4. Replace sizeof type with type
        // Pattern: sizeof type
        // Replacement: type
        // This pattern handles types with namespaces and template parameters
        std::regex sizeof_without_parentheses_regex(R"(sizeof\s+([a-zA-Z_:][a-zA-Z0-9_:<>]*))");
        line = std::regex_replace(line, sizeof_without_parentheses_regex, "$1");

        // Remove comment
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        size_t comment_start = line.find("/*");
        size_t comment_end = line.find("*/");
        if (comment_start != std::string::npos && comment_end != std::string::npos) {
            line = line.substr(0, comment_start) + line.substr(comment_end + 2);
        }
        // Remove empty lines
        if (line.empty()) continue;
        // Remove leading spaces
        std::size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos) { // Check if line is not just whitespace
            std::size_t end = line.find_last_not_of(" \t");
            line = line.substr(start, end - start + 1);
        }

        // Remove const strings between double quotes
        size_t quote_start = line.find("\"");
        size_t quote_end = line.find("\"", quote_start + 1);
        while (quote_start != std::string::npos && quote_end != std::string::npos) {
            line = line.substr(0, quote_start) + line.substr(quote_end + 1);
            quote_start = line.find("\"");
            quote_end = line.find("\"", quote_start + 1);
        }

        output += line + " ";
    }
    return output;
}


void StaticAnalyzer::printTaintMap(const TaintMap& taintMap){
    std::cout << "Taint Map:" << std::endl;
    for (const auto& [funcName, taints] : taintMap) {
        std::cout << "Function: " << funcName << std::endl;
        std::cout << "Tainted variables: ";
        for (const auto& var : taints.first) {
            std::cout << var << " ";
        }
        std::cout << std::endl;
        std::cout << "Tainted parameters: ";
        for (const auto& var : taints.second) {
            std::cout << var << ", ";
        }
        std::cout << "\n====="<< std::endl;
    }
    std::cout << "\n\n\n" << std::endl;
}

void StaticAnalyzer::backwardTaintAnalysis(const std::vector<CodeElement>& stmts,
                                           TaintMap& taintMap,
                                           std::stack<std::pair<std::string, bool>>& functionStack,
                                           std::map<std::string, std::vector<std::string>> definitions,
                                           std::vector<Node*>& nodesInGraph,
                                           std::string currentFunction,
                                           std::string previousFunction,
                                           bool isForward,
                                           std::set<std::string>& taintedVariables,
                                           std::set<std::string>& taintedVariablesPrev,
                                           bool forceTrack,
                                           bool startToTrack){

    int parameterIndex = 0;
    for (auto it = stmts.rbegin(); it != stmts.rend(); it++) {
        CodeElement stmt = *it;
        std::cout << "backward visiting stmt: " << stmt.content << " of type: " << stmt.type << std::endl;
        // start to track the tainted variables when the previous function is called
//        std::cout << "Current tainted paras: ";
//        for (const auto& var : taintMap[currentFunction].second) {
//            std::cout << var << " ";
//        }
//        std::cout << std::endl;

        if(!forceTrack){
            if (stmt.type == "call" && startToTrack == false) {
                std::string funcName = stmt.content.substr(0, stmt.content.find("("));
                if (funcName == previousFunction) {
                    startToTrack = true;
                }
                // visit definition map, if the function is in the definition map
                // then start to track the tainted variables
                if (definitions.find(funcName) != definitions.end()) {
                    for (const auto &def: definitions[funcName]) {
                        if (def == previousFunction) {
                            startToTrack = true;
                        }
                    }
                }
            }
            if (startToTrack == false) {
                continue;
            }
        }

        // std::cout << "stmt: " << stmt.content << " type: " << stmt.type << std::endl;
        variableInfo variables = {};
        functionInfo functionCalls = {};
        std::tie(variables, functionCalls) = extractVariables(stmt.content, stmt.type);
        if (stmt.type == "decl")
        {
            // then put the variable on both left and right side into the tainted variable set
            // but const values like int a = 1, should not be put into the tainted variable set
            for (const auto& var : variables) {
                if (taintedVariables.find(var) != taintedVariables.end()) {
                    // if the variable is already tainted, then put all the variables in the declaration into the tainted variable set
                    for (const auto& var : variables) {
                        // if the variable is a const value, then do not put it into the tainted variable set
                        cout << "Variable: " << var << " is digit: " << isdigit(var) << endl;
                        if (isdigit(var)) {
                            continue;
                        }
                        taintedVariables.insert(var);
                    }
                }
            }
        }else if (stmt.type == "expr") {
            // if the expression contains any tainted variable, then put all the variables in the expression into the tainted variable set
            for (const auto& var : variables) {
                if (taintedVariables.find(var) != taintedVariables.end()) {
                    for (const auto& var : variables) {
                        if (isdigit(var)) {
                            continue;
                        }
                        taintedVariables.insert(var);
                    }
                }
            }
        }else if (stmt.type == "call") {
            // if it is the previous function, extract the tainted variables from the map
            if (stmt.content.substr(0, stmt.content.find("(")) == previousFunction) {
                for (const auto& var : taintedVariablesPrev) {
                    std::cout << "visiting parameter: " << var << std::endl;
                    // the var is like #1, #2, #13, etc.
                    // we need to find the corresponding parameter in the function call
                    if (var[0] == '#') {
                        int varIndex = std::stoi(var.substr(1));
                        // print out all of the variables in the function call
                        for (const auto& var : variables) {
                            std::cout << "Parameter: " << var << std::endl;
                        }
                        std::string arg = variables[varIndex];
                        if (isdigit(arg)) {
                            continue;
                        }
                        taintedVariables.insert(arg);
                    }else if (var[0] == '$') {
                        //TODO: if the variable is a return value, then put all the variables in the function call into the tainted variable set
                        for (const auto& var : variables) {
                            taintedVariables.insert(var);
                            if (isdigit(var)) {
                                continue;
                            }
                        }
                    }

                }
            }else{
                // if it is not the previous function, then put all the variables in the function call into the tainted variable set
                for (const auto& var : variables) {
                    if (isdigit(var)) {
                        continue;
                    }
                    taintedVariables.insert(var);
                }
            }

        }else if (stmt.type == "return") {
            if (taintMap[currentFunction].second.find("$*") != taintMap[currentFunction].second.end()) {
                for (const auto &var: variables) {
                    if (isdigit(var)) {
                        continue;
                    }
                    taintedVariables.insert(var);
                }
            }

            int index = 0;
            for (const auto& var : variables) {
                if (taintedVariables.find(var) != taintedVariables.end()) {
                    std::string varIndex = "$" + std::to_string(index);
                    taintMap[currentFunction].second.insert(varIndex);
                }
                index++;
            }

        }else if (stmt.type == "parameter") {
            // if the parameter contains any tainted variable,
            // then put all the variables in the parameter into the tainted variable set
            for (const auto& var : variables) {
                std::cout << "Parameter: " << var << std::endl;
                if (taintedVariables.find(var) != taintedVariables.end()) {
                    std::string varIndex = "#" + std::to_string(parameterIndex);
                    taintMap[currentFunction].second.insert(varIndex);
                    std::cout << "Tainted parameter: " << var << " index: " << varIndex << std::endl;
                }
                parameterIndex++;
            }

        }
    }
    for (const auto& var : taintedVariables) {
        taintMap[currentFunction].first.insert(var);
    }

}



void StaticAnalyzer::forwardTaintAnalysis(const std::vector<CodeElement>& stmts,
                          TaintMap& taintMap,
                          std::stack<std::pair<std::string, bool>>& functionStack,
                          std::map<std::string, std::vector<std::string>> definitions,
                          std::vector<Node*>& nodesInGraph,
                          std::string currentFunction,
                          std::string previousFunction,
                          bool isForward,
                          std::set<std::string>& taintedVariables,
                          std::set<std::string>& taintedVariablesPrev,
                          bool forceTrack,
                          bool startToTrack){

    vector<string> parameters = {};
    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
        CodeElement stmt = *it;
        std::cout << "forward visiting stmt: " << stmt.content << " of type: " << stmt.type << std::endl;

        // start to track the tainted variables when the previous function is called
        if (stmt.type == "call" && startToTrack == true) {
            std::string funcName = stmt.content.substr(0, stmt.content.find("("));
            if (funcName == previousFunction) {
                startToTrack = false;
            }
        }
        if (startToTrack == false) {
            continue;
        }


        // std::cout << "stmt: " << stmt.content << " type: " << stmt.type << std::endl;
        variableInfo variables = {};
        functionInfo functionCalls = {};

        std::tie(variables, functionCalls) = extractVariables(stmt.content, stmt.type);

        if (stmt.type == "decl"){

            // then put the variable on both left and right side into the tainted variable set
            // but const values like int a = 1, should not be put into the tainted variable set
            for (const auto& var : variables) {

                if (taintedVariables.find(var) != taintedVariables.end()) {
                    // if the variable is already tainted, then put all the variables in the declaration into the tainted variable set
                    for (const auto& var : variables) {
                        // if the variable is a const value, then do not put it into the tainted variable set
                        if (isdigit(var)) {
                            continue;
                        }
                        taintedVariables.insert(var);
                        for (const auto&functionCall : functionCalls) {
                            // if the functioncall doesn't have any arguments, isNextFunctionForward is false
                            bool hasArguments = functionCall.second;
                            std::string functionName = functionCall.first;
                            std::cout << "Pushing function: " << functionName << " with hasArguments: " << hasArguments << std::endl;
                            functionStack.push({functionName, hasArguments});
                            if (!hasArguments){
                                // taint all of the return value of the function call
                                std::string returnValue = "$*";
                                taintMap[functionName].second.insert(returnValue);
                            }
                        }
                        //std::cout << "Added Tainted variable: " << var << std::endl;
                    }
                }
            }
        }else if (stmt.type == "expr") {
            // if the expression contains any tainted variable, then put all the variables in the expression into the tainted variable set
            for (const auto& var : variables) {
                if (taintedVariables.find(var) != taintedVariables.end()) {
                    for (const auto& var : variables) {
                        if (isdigit(var)) {
                            continue;
                        }
                        taintedVariables.insert(var);
                        //std::cout << "Added Tainted variable: " << var << std::endl;
                    }
                }
            }
        }else if (stmt.type == "call") {
            // if the function call contains any tainted variable,
            // then put all the args in the function call into the tainted variable set
            std::string calledfuncName = stmt.content.substr(0, stmt.content.find("("));

            // if the function is not in the nodesInGraph, then skip it
            bool isFunctionInGraph = false;
            for (const auto& node : nodesInGraph) {
                if (node->get_name() == calledfuncName) {
                    isFunctionInGraph = true;
                    break;
                }
            }
            if (isFunctionInGraph == false) {
                continue;
            }

            // if it is the graph, keep track of the function call
            std::vector<std::string> arguments = extractFromCall(stmt.content);
            std::cout << "Function call: " << calledfuncName
                      << " arguments: ";
            for (const auto& arg : arguments) {
                std::cout << arg << " ";
            }
            std::cout << std::endl;


            //print current function's tainted variables
            std::cout << "Current function: " << currentFunction << std::endl;
            std::cout << "Tainted variables: ";
            for (const auto& var : taintedVariables) {
                std::cout << var << " ";
            }
            std::cout << std::endl;

            int index = 0;
            for (const auto& arg : arguments) {
                if (taintedVariables.find(arg) != taintedVariables.end()) {
                    std::string varIndex = "#" + std::to_string(index);
                    taintMap[calledfuncName].second.insert(varIndex);
                    bool isNextFunctionForward = true;
                    std::cout << "Pushing function: " << calledfuncName << " with hasArguments: " << isNextFunctionForward << std::endl;
                    functionStack.push({calledfuncName, isNextFunctionForward});
                    break;
                }
                index++;
            }

        }else if (stmt.type == "return") {
        }else if (stmt.type == "parameter") {

            vector<int> taintedParaIndex = {};
            for (const auto& taintIndex : taintMap[currentFunction].second) {
                int index = std::stoi(taintIndex.substr(1));
                taintedParaIndex.push_back(index);
            }


            for (const auto& var : variables) {
                parameters.push_back(var);
                int parameterIndex = parameters.size() - 1;
                if (std::find(taintedParaIndex.begin(), taintedParaIndex.end(), parameterIndex) != taintedParaIndex.end()) {
                    if (isdigit(var)) {
                        continue;
                    }
                    taintedVariables.insert(var);
                    //std::cout << "Added Tainted variable: " << var << std::endl;
                }
            }


        }
    }
    for (const auto& var : taintedVariables) {
        taintMap[currentFunction].first.insert(var);
    }

}





void StaticAnalyzer::TaintAnalysis(const std::vector<CodeElement>& stmts,
                                   TaintMap& taintMap,
                                   std::stack<std::pair<std::string, bool>>& functionStack,
                                   std::map<std::string, std::vector<std::string>> definitions,
                                   std::vector<Node*>& nodesInGraph,
                                   std::string currentFunction,
                                   std::string previousFunction,
                                   bool isForward,
                                   bool forceTrack) {
    bool startToTrack = isForward ? true : false;
    std::set<std::string> taintedVariables;
    std::set<std::string> taintedVariablesPrev;
    if (taintMap.find(currentFunction) == taintMap.end()) {
        taintMap[currentFunction] = std::pair<std::set<std::string>, std::set<std::string>>();
    }else{
        taintedVariables = taintMap[currentFunction].first;
    }

    std::cout << "Current function: " << currentFunction << std::endl;
    std::cout << "Previous function: " << previousFunction << std::endl;
    std::cout << "isForward: " << isForward << std::endl;
    taintedVariablesPrev = taintMap[previousFunction].second;


    if (isForward == false){
        backwardTaintAnalysis(stmts, taintMap, functionStack, definitions, nodesInGraph, currentFunction, previousFunction, isForward, taintedVariables, taintedVariablesPrev, forceTrack, startToTrack);
    }else if(isForward == true) {
        forwardTaintAnalysis(stmts, taintMap, functionStack, definitions, nodesInGraph, currentFunction, previousFunction, isForward, taintedVariables, taintedVariablesPrev, forceTrack, startToTrack);
    }

    printTaintMap(taintMap);
    std::cout << "functionStack size: " << functionStack.size() << std::endl;

}
