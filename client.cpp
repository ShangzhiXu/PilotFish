//
// Created by mxu49 on 2024/8/13.
// g++ -o srcML_test srcML_test.cpp `xml2-config --cflags --libs`
//
#include <cstring>
#include <iostream>
#include <stack>
#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include "drsyms.h"
#include "graph.h"

#define MAX_CALL_DEPTH 256

std::set<std::string> function_names;
static int num_functions = 0;

static Graph* call_graph;
static std::stack<const char*> call_stack;

struct symbol_filter_data_t {
    const char *func_name; // Function name to wrap
    app_pc module_start;   // Start address of the module
};


static void event_exit(void);

static void generic_wrap_pre(void *wrapcxt, void **user_data) {
    const char *func_name = (const char *)*user_data;
    //dr_printf("Function %s is called\n", func_name);  // Added logging
    call_stack.push(func_name);
}

static void generic_wrap_post(void *wrapcxt, void *user_data) {
    const char *func_name = (const char *)user_data;
    if (call_stack.empty()) {
        return;
    }
    const char* current = call_stack.top();

    call_stack.pop();
    if (call_stack.empty()) {
        return;
    }
    const char* caller = call_stack.top();
    std::stack<const char*> temp = call_stack;

    call_graph->addCall(caller, current);

}

void load_function_names(const char *filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(file, line)) {

        if (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
        }


        size_t paren_pos = line.find('(');
        std::string func_name;
        if (paren_pos != std::string::npos) {
            func_name = line.substr(0, paren_pos);
        } else {
            func_name = line;
        }


        size_t start = func_name.find_first_not_of(" \t\r\n");
        size_t end = func_name.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            func_name = func_name.substr(start, end - start + 1);
        } else {
            func_name = "";
        }

        if (!func_name.empty()) {

            function_names.insert(func_name);
        }
    }

    file.close();
}

/* Callback function to filter and wrap symbols */
static bool symbol_filter(drsym_info_t *info, drsym_error_t status, void *data) {

    if (function_names.find(info->name) == function_names.end()) {
        return true;
    }

    app_pc start = (app_pc)data; // Assuming data is the start address of the module
    app_pc func_pc = start + info->start_offs; // Correct pointer arithmetic
    // Wrap
    // get the function name
    auto it = function_names.find(info->name);
    const char* func_name = it->c_str();
    drwrap_wrap_ex(func_pc, generic_wrap_pre, generic_wrap_post, (void *)func_name, 0);
   // dr_printf("Wrapped function: %s at address: %p\n", info->name, func_pc);

    return true;
}

/* Event called when module is loaded */
static void module_load_event(void *drcontext, const module_data_t *mod, bool loaded) {
    if (loaded) {
        drsym_error_t sym_result;
        std::cout << "Module loaded: " << mod->full_path << std::endl;


        sym_result = drsym_enumerate_symbols_ex(mod->full_path, symbol_filter, sizeof(drsym_info_t), (void*)mod->start, DRSYM_DEMANGLE);
        if (sym_result != DRSYM_SUCCESS) {
            dr_printf("Failed to enumerate symbols for module %s\n", mod->full_path);
        }
    }
}


DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {
    if (argc < 5) {
        dr_printf("Usage: %s <function_names_file> <backtrace_file> <define.json> <pollution_info>\n", argv[0]);
        return;
    }
    //print all of the arguments
    for (int i = 0; i < argc; i++) {
        dr_printf("Argument %d: %s\n", i, argv[i]);
    }

    load_function_names(argv[1]);

    dr_set_client_name("Multi-Function Wrapping Client", "http://dynamorio.org");
    drmgr_init();
    drwrap_init();
    if (drsym_init(0) != DRSYM_SUCCESS) {
        dr_printf("Failed to initialize symbol lookup\n");
        dr_exit_process(1);
    }
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
    call_graph = new Graph();

    call_graph->parseASanOutput(argv[2]);
    std::cout << "Backtrace loaded" << std::endl;

    call_graph->loadDefineJson(argv[3]);
    std::cout << "Define JSON loaded" << std::endl;

    call_graph->loadPollutionInfo(argv[4]);
    std::cout << "Pollution information loaded" << std::endl;

    call_graph->addBacktrace();

}

static void event_exit(void) {
    dr_printf("Client 'my_client' exiting\n");
    call_graph->removeInterceptors();
    call_graph->printGraph();
    const module_data_t *main_module = dr_get_main_module();
    if (main_module == NULL) {
        dr_printf("Failed to get main module\n");
        dr_exit_process(1);
    }else{
        const char* binary_path = main_module->full_path;

        call_graph->Traversal(binary_path);
    }



    delete call_graph;
    drwrap_exit();
    drmgr_exit();
    drmgr_exit();
}

