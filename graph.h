//
// Created by mxu49 on 2024/8/13.
//

#ifndef DYNAMORIO_GRAPH_H
#define DYNAMORIO_GRAPH_H

#include "node.h"
#include "staticAnalyzer.h"
#include <algorithm>  // Required for std::find
#include <vector>
#include <string>
#include <iostream>   // Required for std::cout
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <cstdio>   // printf, fprintf
#include <cstdlib>  // malloc, free
#include <cstring>  // strlen, strcpy
#include <sstream>
#include <set>
#include <map>
// include json
#include <nlohmann/json.hpp>

using namespace std;
struct FunctionInfo {
    std::string function_name;
    std::string file_name;
    int line_number;
};


typedef std::vector<Node*> Path;
class Graph {
private:
    std::vector<Node*> list;
    std::vector<FunctionInfo> backtrace;
    std::map<std::string, std::vector<std::string>> definitions;
    struct PollutionInfo {
        std::set<std::string> var;
        std::set<std::string> index;
    };

    std::map<std::string, PollutionInfo> pollutionInfos;
    int size;
public:
    Graph();
    ~Graph();
    void addCall(const char* caller, const char* callee);
    void printGraph();
    std::unordered_map<Node*, Path> reverseGraph();
    void dfs(Node* node, Path& path, std::vector<Path>& allPaths, const std::unordered_map<Node*, Path>& reversed, std::vector<Node*>& visited );
    std::vector<Path> findAllCallChains(Node* target);
    Node* findNode(const char* name);
    void Traversal(const char* binary_name);
    void visitPath(Path path, StaticAnalyzer& staticAnalyzer,const char *binary_name, TaintMap& taintMap,bool isForward);
    std::vector<CodeElement> awkElementExtraction(std::string file_name, StaticAnalyzer& analyzer, int start_line_number, int end_column_number);
    void addNode(const char* name);
    int getSize();
    void parseASanOutput(std::string asan_output_file);
    void loadDefineJson(std::string define_json_file);
    void loadPollutionInfo(std::string pollution_info_file);
    std::vector<std::string> splitBySpace(const std::string &line);
    void addBacktrace();
    void removeInterceptors();

};

#endif //DYNAMORIO_GRAPH_H
