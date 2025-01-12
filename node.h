//
// Created by mxu49 on 2024/8/13.
//

#ifndef DYNAMORIO_NODE_H
#define DYNAMORIO_NODE_H

#define MAX_FUNCTIONS 1000
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>

using namespace std;
class Node{
private:
    string name;
    std::vector<Node*> next;
    std::vector<int> next_call_count;
    int call_count;
public:
    Node();
    ~Node();
    std::string get_name();
    void change_name(std::string name);
    std::vector<Node*> get_next();
    int get_call_count();
    int get_call_count(std::string name);
    void set_name(std::string name);
    void set_next(Node* next);
    void increment_call_count();
};



#endif //DYNAMORIO_NODE_H
