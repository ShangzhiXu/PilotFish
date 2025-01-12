//
// Created by mxu49 on 2024/8/13.
//

#include "node.h"
#define MAX_CALL_DEPTH 256

// start to implement the Node functions
Node::Node() {
    this->next = std::vector<Node*>();
    this->call_count = 0;
    this->name = "";
}

Node::~Node() {
}

void Node::set_name(std::string name) {
    this->name = name;
}

void Node::set_next(Node* nextNode) {
    //std::cout << "In set next, Adding call from " << this->name << " to " << nextNode->get_name() << std::endl;
    nextNode->increment_call_count();
    for (int i = 0; i < this->next.size(); i++) {
        if (this->next[i]->get_name() == nextNode->get_name()) {
            this->next_call_count[i]++;
            return;
        }
    }
    this->next.push_back(nextNode);
    this->next_call_count.push_back(1);
}

std::string Node::get_name() {
    return this->name;
}

std::vector<Node*> Node::get_next() {
    return this->next;
}

int Node::get_call_count() {
    return this->call_count;
}

int Node::get_call_count(std::string name) {
    for (int i = 0; i < this->next.size(); i++) {
        if (this->next[i]->get_name() == name) {
            return this->next_call_count[i];
        }
    }
    return 0;
}

void Node::increment_call_count() {
    this->call_count++;
}

void Node::change_name(std::string name) {
    this->name = name;
}