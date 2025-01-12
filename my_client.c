#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FUNCTIONS 100
#define MAX_CALL_DEPTH 256

typedef struct Node {
    char name[50];
    struct Node* next;
} Node;

typedef struct Node* (*NodeFactory)(const char* name);

Node* newNode(const char* name) {
    Node* node = (Node*)malloc(sizeof(Node));
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->next = NULL;
    return node;
}

typedef struct {
    char name[50];
    Node* head;
    void (*addNode)(struct AdjList* this, const char* callee);
} AdjList;

void addNodeToList(AdjList* this, const char* callee) {

    Node* new_node = newNode(callee);
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

typedef struct {
    AdjList list[MAX_FUNCTIONS];
    int size;
    void (*addCall)(struct Graph* this, const char* caller, const char* callee);
} Graph;

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

typedef struct {
    const char* stack[MAX_CALL_DEPTH];
    int depth;
    void (*push)(struct CallStack* this, const char* func_name);
    const char* (*pop)(struct CallStack* this);
    const char* (*top)(struct CallStack* this);
} CallStack;

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
        printf("%s calls: ", graph->list[i].name);
        Node* current = graph->list[i].head;
        while (current != NULL) {
            printf("%s ", current->name);
            current = current->next;
        }
        printf("\n");
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
static __thread int call_stack_depth = 0;

static char **function_names;
static int num_functions = 0;

static void event_exit(void);
static void generic_wrap_pre(void *wrapcxt, void **user_data);
static void generic_wrap_post(void *wrapcxt, void *user_data);

void load_function_names(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Unable to open file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[256];  // Buffer for each line/function name
    int capacity = 20;  // Initial capacity for the array
    function_names = malloc(capacity * sizeof(char*));
    if (function_names == NULL) {
        perror("Failed to allocate memory for function names");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), file)) {
        if (num_functions >= capacity) {
            capacity *= 2;
            function_names = realloc(function_names, capacity * sizeof(char*));
            if (function_names == NULL) {
                perror("Failed to reallocate memory for function names");
                exit(EXIT_FAILURE);
            }
        }
        line[strcspn(line, "\n")] = '\0';  // Remove newline character
        function_names[num_functions++] = strdup(line);  // Duplicate and store the line
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
    load_function_names(argv[1]);  // Load function names from file
    dr_set_client_name("Multi-Function Wrapping Client", "http://dynamorio.org");
    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
    call_graph = createGraph();
    call_stack = NULL;
}

static void event_exit(void) {
    dr_printf("Client 'my_client' exiting\n");
    // Print the call graph, free graph, and other cleanup tasks
    print_graph(call_graph);  // Assume print_graph is a function to print the graph
    free_graph(call_graph);   // Clean up the graph
    free(call_stack);         // Free call stack

    drwrap_exit();            // Clean up the wrapper extension
    drmgr_exit();             // Clean up the manager extension
}
static void generic_wrap_pre(void *wrapcxt, void **user_data) {
    const char *func_name = (const char *)*user_data;
    //dr_printf("Entering function: %s\n", func_name);
    if (call_stack == NULL) {
        call_stack = createCallStack();
    }
    call_stack->push(call_stack, func_name);


}

static void generic_wrap_post(void *wrapcxt, void *user_data) {
    const char *func_name = (const char *)user_data;
    int retval = (int)drwrap_get_retval(wrapcxt);
    //dr_printf("Exiting function: %s, Return value: %d\n", func_name, retval);

    const char* current = call_stack->pop(call_stack);
    printf("current: %s\n", current);
    const char* caller = call_stack->top(call_stack);
    printf("caller: %s\n", caller);
    if (caller != NULL) {
        call_graph->addCall(call_graph, caller, current);
    }


}
