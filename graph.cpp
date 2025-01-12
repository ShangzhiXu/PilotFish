//
// Created by mxu49 on 2024/8/13.
//
//

#include "graph.h"
#include <iostream>

using namespace std;
Graph::Graph() {
    this->size = 0;
}

Graph::~Graph() {
}





void Graph::addCall(const char* caller, const char* callee) {
    Node* callerNode = this->findNode(caller);
    Node* calleeNode = this->findNode(callee);
    //std::cout << "Adding call from " << caller << " to " << callee << std::endl;
    if (callerNode == NULL) {
        this->addNode(caller);
        callerNode = this->findNode(caller);
    }
    if (calleeNode == NULL) {
        this->addNode(callee);
        calleeNode = this->findNode(callee);
    }
    callerNode->set_next(calleeNode);
    //std::cout << "Adding call from " << caller << " to " << callee << " done" << std::endl;
}



void Graph::printGraph() {

    for (int  i = 0; i < this->list.size(); i++) {

        std::vector<Node*> nodes = this->list[i]->get_next();
        if (nodes.size() == 0) {
            continue;
        }
        std::cout << "Node " << this->list[i]->get_name() << std::endl;
        for (int j = 0; j < nodes.size(); j++) {
            std::cout << nodes[j]->get_name() << " "
                      << this->list[i]->get_call_count(nodes[j]->get_name())
                      << std::endl;
        }
        std::cout << std::endl;
    }
}

void Graph::loadDefineJson(std::string define_json_file) {
    std::ifstream file(define_json_file);
    if (!file.is_open()) {
        std::cerr << "Error: unable to open file " << define_json_file << std::endl;
        return;
    }

    nlohmann::json j;
    file >> j;
    file >> j;
    for (const auto& item : j.items()) {
        std::string key = item.key();
        std::vector<std::string> values = item.value();
        definitions[key] = values;
    }
    file.close();
}

void Graph::loadPollutionInfo(std::string pollution_info_file) {
    std::ifstream file(pollution_info_file);
    if (!file.is_open()) {
        std::cerr << "Error: unable to open file " << pollution_info_file << std::endl;
        return;
    }

    nlohmann::json j;
    file >> j;
    for (const auto& item : j.items()) {
        std::string key = item.key();
        PollutionInfo info;
        for (const auto& var : item.value()["var"]) {
            info.var.insert(var);
        }
        for (const auto& index : item.value()["index"]) {
            info.index.insert(index);
        }
        pollutionInfos[key] = info;
    }
    file.close();
}

std::unordered_map<Node*, Path> Graph::reverseGraph() {
    std::unordered_map<Node*, Path> reversed;
    std::cout << "Constructing Reversed Graph" << std::endl;
    for (Node* node : this->list) {
        for (Node* adj : node->get_next()) {
            reversed[adj].push_back(node);
        }
        if (reversed.find(node) == reversed.end()) {
            reversed[node]; // Ensure all nodes are in the reversed graph even if they have no predecessors
        }
    }
    std::cout << "Reversed Graph" << std::endl;
    return reversed;
}

void Graph::dfs(Node* node, Path& path, std::vector<Path>& allPaths,
         const std::unordered_map<Node*, Path>& reversed, std::vector<Node*>& visited ) {
    //if there are more than one time the same node in the path, return
    // we only allow the cycle to be executed once
    if (std::count(path.begin(), path.end(), node) > 1) {
        for (Node* n : path) {
            std::cout << n->get_name() << " -> ";
        }
        std::cout << "CYCLE" << std::endl;
        return;
    }
    path.push_back(node);
    if (reversed.at(node).empty() || node->get_name() == "main") {  // No predecessors or it is the start node
        allPaths.push_back(std::vector<Node*>(path.rbegin(), path.rend()));
    } else {
        for (Node* prev : reversed.at(node)) {
            dfs(prev, path, allPaths, reversed, visited);
        }
    }

    path.pop_back();
}

std::vector<Path> Graph::findAllCallChains(Node* target) {
    auto reversed = reverseGraph();
    Path currentPath;
    std::vector<Node*> visited;
    std::vector<Path> allPaths;
    dfs(target, currentPath, allPaths, reversed, visited);
    return allPaths;
}

Node* Graph::findNode(const char* name) {
    for (int i = 0; i < this->size; i++) {
        if (this->list[i]->get_name() == name) {
            return this->list[i];
        }
    }
    return NULL;
}

void Graph::Traversal(const char *binary_name)  {

    // extract the function sequence
    StaticAnalyzer analyzer;
    std::cout << "Backward Traversal" << std::endl;

    //TODO: find the crash point and extract source to put into the taintMap
    Node* target = this->findNode("strcpy");

    if (target == NULL) {
        std::cout << "Target not found" << std::endl;
        return;
    }
    std::vector<Path> allPaths = findAllCallChains(target);
    // print the function sequences
    for (int i = 0; i < allPaths.size(); i++) {
        for (int j = 0; j < allPaths[i].size(); j++) {
            std::cout << allPaths[i][j]->get_name() << " -> ";
        }
        std::cout << std::endl;
    }

    // Do backward traversal with srcML
    TaintMap taintMap;

//    taintMap["_tinydir_strcpy"].first = {"dir_name_buf", "path"};
//    taintMap["_tinydir_strcpy"].second = {"#0","#1"};

    for (auto it = pollutionInfos.begin(); it != pollutionInfos.end(); it++) {
        std::string var = it->first;
        PollutionInfo info = it->second;
        taintMap[var].first = info.var;
        taintMap[var].second = info.index;
    }


    for (int i = 0; i < allPaths.size(); i++) {
        visitPath(allPaths[i], analyzer, binary_name, taintMap, false);
    }

    std::cout << "\n\n\n\n\nForward Traversal" << std::endl;
    // Do forward traversal with srcML
    for (int i = 0; i < allPaths.size(); i++) {
        visitPath(allPaths[i], analyzer, binary_name, taintMap, true);
    }
}

std::vector<CodeElement> Graph::awkElementExtraction(std::string file_name, StaticAnalyzer& analyzer, int start_line_number, int end_column_number) {
    //  use awk to extract the source code of the function
    //         then preprocess the code to remove comments etc.
    std::string awk_command = "awk 'NR>=" + std::to_string(start_line_number) + " && NR<=" + std::to_string(end_column_number) + "' " + file_name;
    std::string awk_output = analyzer.exec(awk_command.c_str());
//    std::cout << "awk command: " << awk_command << std::endl;
//    std::cout << "awk output: " << awk_output << std::endl;
    //strip the leading meaningless characters, start with the definition of the function
    size_t pos = awk_output.find_first_not_of('}');
    if (pos != std::string::npos) {
        awk_output = awk_output.substr(pos);
    }
    awk_output = analyzer.preprocessCode(awk_output);

    // parse the source code of the function with srcML
    std::string command = "echo \"" + awk_output + "\" | srcml --language=C++ --position  ";
    std::string srcml_output = analyzer.exec(command.c_str());

    // parse the srcML output to get the effected elements
    xmlDocPtr doc = xmlReadMemory(srcml_output.c_str(), srcml_output.size(), NULL, NULL, 0);
    xmlNodePtr root_element = xmlDocGetRootElement(doc);

    std::vector<CodeElement> elements = analyzer.parseElements(root_element);
    return elements;
}

void Graph::visitPath(Path path, StaticAnalyzer& analyzer, const char *binary_name, TaintMap& taintMap, bool isForward) {
    // in visitPath, we need to extract the function sequence backwardly
    std::string currentFunction = "";
    std::string previousFunction = "";
    std::stack<std::pair<std::string, bool>> functionStack = std::stack<std::pair<std::string, bool>>();

    int path_size = path.size();
    if (isForward == false) {
        std::reverse(path.begin(), path.end());
    }
    for(int i = 0; i < path_size; i++) {
        currentFunction = path[i]->get_name();
        if (isForward == true && i+1 < path_size) {
            previousFunction = path[i+1]->get_name();
        }
        std::cout << "Visiting " << currentFunction << std::endl;
        // Step 1: get the location of the function in the source code
        std::tuple<std::string, int, int> func_info;
        func_info = analyzer.getFunctionInfo(binary_name, currentFunction);
        if (std::get<0>(func_info) == "") {
            previousFunction = currentFunction;
            continue;
        }
        std::string file_name = std::get<0>(func_info);
        int start_line_number = std::get<1>(func_info);
        int end_column_number = std::get<2>(func_info);

        std::vector<CodeElement> elements = awkElementExtraction( file_name, analyzer, start_line_number, end_column_number);
        std::cout << "elements size: " << elements.size() << std::endl;
        analyzer.TaintAnalysis(elements, taintMap, functionStack, this->definitions, this->list, currentFunction, previousFunction, isForward);

        if (isForward == false) {
            previousFunction = currentFunction;
        }
    }


    std::cout << "Function stack size: " << functionStack.size() << std::endl;


    while (!functionStack.empty()) {
        std::string currentFunction = "";
        bool isForward = false;
        std::tie(currentFunction, isForward) = functionStack.top();
        functionStack.pop();
        std::cout << "Visiting " << currentFunction << std::endl;
        // Step 1: get the location of the function in the source code
        std::tuple<std::string, int, int> func_info;
        func_info = analyzer.getFunctionInfo(binary_name, currentFunction);
        if (std::get<0>(func_info) == "") {
            previousFunction = currentFunction;
            continue;
        }
        std::string file_name = std::get<0>(func_info);
        int start_line_number = std::get<1>(func_info);
        int end_column_number = std::get<2>(func_info);

        std::vector<CodeElement> elements = awkElementExtraction(file_name, analyzer, start_line_number, end_column_number);
        analyzer.TaintAnalysis(elements, taintMap, functionStack, this->definitions, this->list, currentFunction, previousFunction, isForward, true);
    }
}

void Graph::addNode(const char* name) {
    //std::cout << "Adding node " << name << std::endl;
    Node* new_node = new Node();
    new_node->set_name(name);
    this->list.push_back(new_node);
    this->size++;
    //std::cout << "Adding node " << name << " done with size " << this->size << std::endl;
}

int Graph::getSize() {
    return this->list.size();
}

std::vector<std::string> Graph::splitBySpace(const std::string &line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to parse AddressSanitizer output
void Graph::parseASanOutput(std::string asan_output_file) {
    std::ifstream file(asan_output_file);  // Corrected variable name for file stream
    if (!file) {
        std::cerr << "Failed to open file: " << asan_output_file << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        // remove the leading spaces
        line = line.substr(line.find_first_not_of(" \t"));
        // if there is ( in the function name, remove the part between ( and )
        size_t paren_pos_left = line.find('(');
        size_t paren_pos_right = line.find(')');
        if (paren_pos_left != std::string::npos && paren_pos_right != std::string::npos) {
            // remove the part between ( and )
            line = line.substr(0, paren_pos_left) + line.substr(paren_pos_right + 1);
        }

        std::vector <std::string> parts = splitBySpace(line);
        printf("Line: %s parts size: %d\n", line.c_str(), parts.size());
        // Check for a line that contains "in" and has at least 6 parts
        if (parts.size() >= 4) {
            FunctionInfo info;
            info.function_name = parts[3];   // Function name

            info.file_name = "";
            info.line_number = 0;
            backtrace.push_back(info);
            printf("Function: %s, File: %s, Line: %d\n", info.function_name.c_str(), info.file_name.c_str(), info.line_number);
        }
    }

}

void Graph::addBacktrace() {
   //reverse the backtrace
    std::reverse(backtrace.begin(), backtrace.end());
    for (int i = 0; i < backtrace.size(); i++) {
        if (i + 1 < backtrace.size()) {
            addCall(backtrace[i].function_name.c_str(), backtrace[i + 1].function_name.c_str());
            printf("Adding call from %s to %s\n", backtrace[i].function_name.c_str(), backtrace[i + 1].function_name.c_str());
        }
    }
}

void Graph::removeInterceptors() {
    std::cout << "Removing interceptors" << std::endl;
    for (int i = 0; i < this->list.size(); i++) {
        std::cout << "Checking " << this->list[i]->get_name() << std::endl;
        if (this->list[i]->get_name().find("__interceptor_") != std::string::npos) {
            // remove "__interceptor_" from the function name
            this->list[i]->change_name(this->list[i]->get_name().substr(14));
        }
    }

}


