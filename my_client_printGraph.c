#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FUNCTIONS 100
#define MAX_CALL_DEPTH 256

// Declare globally
static char **function_names;
static int num_functions = 0;


struct Node {
    char name[50];
    struct Node* next;
};
typedef struct Node Node;

struct  AdjList{
    char name[50];
    Node* head;
    void (*addNode)(struct AdjList* this, const char* callee);
};
typedef struct AdjList AdjList;

void addNodeToList(AdjList* this, const char* callee) {
    Node* current = this->head;
    while (current != NULL) {
        if (strcmp(current->name, callee) == 0) {
            return; // Duplicate callee found, do not add again
        }
        current = current->next;
    }

    Node* new_node = (Node*)malloc(sizeof(Node));
    strncpy(new_node->name, callee, sizeof(new_node->name) - 1);
    new_node->next = this->head;
    this->head = new_node;
}

AdjList createAdjList(const char* name) {
    AdjList list;
    strncpy(list.name, name, sizeof(list.name) - 1);
    list.head = NULL;
    list.addNode = addNodeToList;
    return list;
}

struct Graph{
    AdjList list[MAX_FUNCTIONS];
    int size;
    void (*addCall)(struct Graph* this, const char* caller, const char* callee);
} ;
typedef struct Graph Graph;
void addCallToGraph(Graph* this, const char* caller, const char* callee) {
    int caller_idx = -1;
    for (int i = 0; i < this->size; i++) {
        if (strcmp(this->list[i].name, caller) == 0) {
            caller_idx = i;
            break;
        }
    }
    if (caller_idx == -1) {
        caller_idx = this->size++;
        this->list[caller_idx] = createAdjList(caller);
    }
    this->list[caller_idx].addNode(&this->list[caller_idx], callee);
}

Graph* createGraph() {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    graph->size = 0;
    graph->addCall = addCallToGraph;
    return graph;
}

struct CallStack {
    const char* stack[MAX_CALL_DEPTH];
    int depth;
    void (*push)(struct CallStack* this, const char* func_name);
    const char* (*pop)(struct CallStack* this);
    const char* (*top)(struct CallStack* this);
};
typedef struct CallStack CallStack;


void pushToStack(CallStack* this, const char* func_name) {
    if (this->depth < MAX_CALL_DEPTH) {
        this->stack[this->depth++] = func_name;
    }
}

const char* popFromStack(CallStack* this) {
    if (this->depth > 0) {
        return this->stack[--this->depth];
    }
    return NULL;
}

const char* topOfStack(CallStack* this) {
    if (this->depth > 0) {
        return this->stack[this->depth - 1];
    }
    return NULL;
}

CallStack* createCallStack() {
    CallStack* stack = (CallStack*)malloc(sizeof(CallStack));
    stack->depth = 0;
    stack->push = pushToStack;
    stack->pop = popFromStack;
    stack->top = topOfStack;
    return stack;
}

void print_graph(Graph* graph) {
    printf("\n\nCall Graph:\n");
    for (int i = 0; i < graph->size; i++) {
        printf("%s calls:\n", graph->list[i].name);
        Node* current = graph->list[i].head;
        //reverse the list
        Node* prev = NULL;
        Node* next = NULL;
        while (current != NULL) {
            next = current->next;
            current->next = prev;
            prev = current;
            current = next;
        }
        current = prev;
        while (current != NULL) {
            printf("\t%s\n", current->name);
            current = current->next;
        }

    }
}

void free_graph(Graph* graph) {
    for (int i = 0; i < graph->size; i++) {
        Node* current = graph->list[i].head;
        while (current != NULL) {
            Node* temp = current;
            current = current->next;
            free(temp);
        }
    }
    free(graph);
}

static Graph* call_graph;
static __thread CallStack* call_stack;

static void event_exit(void);
static void generic_wrap_pre(void *wrapcxt, void **user_data);
static void generic_wrap_post(void *wrapcxt, void *user_data);

void load_function_names(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Unable to open file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[256];
    int capacity = 20;
    function_names = (char**)malloc(capacity * sizeof(char*));
    if (function_names == NULL) {
        perror("Failed to allocate memory for function names");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        for (int i = 0; i < num_functions; i++) {
            if (strcmp(line, function_names[i]) == 0) {
                goto next_line; // Skip duplicate function names
            }
        }
        if (num_functions >= capacity) {
            capacity *= 2;
            char **new_space = (char**)realloc(function_names, capacity * sizeof(char*));
            if (new_space == NULL) {
                perror("Failed to reallocate memory for function names");
                exit(EXIT_FAILURE);
            }
            function_names = new_space;
        }
        function_names[num_functions++] = strdup(line);
        next_line:;
    }

    fclose(file);
}

static void module_load_event(void *drcontext, const module_data_t *mod, bool loaded) {
    for (int i = 0; i < num_functions; i++) {
        app_pc towrap = (app_pc)dr_get_proc_address(mod->handle, function_names[i]);
        if (towrap != NULL) {
            drwrap_wrap_ex(towrap, generic_wrap_pre, generic_wrap_post, (void *)function_names[i], 0);
        }
    }
}

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {
    if (argc < 2) {
        dr_printf("Usage: %s <function_names_file>\n", argv[0]);
        return;
    }
    load_function_names(argv[1]);
    dr_set_client_name("Multi-Function Wrapping Client", "http://dynamorio.org");
    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
    call_graph = createGraph();
    call_stack = createCallStack();
}

static void event_exit(void) {
    dr_printf("Client 'my_client' exiting\n");
    print_graph(call_graph);
    free_graph(call_graph);
    free(call_stack);
    drwrap_exit();
    drmgr_exit();
}

static void generic_wrap_pre(void *wrapcxt, void **user_data) {
    const char *func_name = (const char *)*user_data;
    if (call_stack->depth == 0 || strcmp(func_name, call_stack->top(call_stack)) != 0) {
        call_stack->push(call_stack, func_name);
    }
}

static void generic_wrap_post(void *wrapcxt, void *user_data) {
    const char *func_name = (const char *)user_data;
    const char* current = call_stack->pop(call_stack);
    const char* caller = call_stack->top(call_stack);
    if (caller != NULL && strcmp(current, caller) != 0) { // Prevent self-calling loops from duplicating
        call_graph->addCall(call_graph, caller, current);
    }
}
