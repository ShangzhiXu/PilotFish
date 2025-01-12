#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include "drsyms.h"
#include <cstring>
#include <iostream>
/* Structure to store function names and their wrapped status */
typedef struct {
    const char *func_name; // Function name to wrap
    app_pc module_start;   // Start address of the module
} function_data_t;

/* List of functions you want to wrap */
static char **function_names;
static int num_functions = 0;
/* Example generic pre and post wrap functions */
void generic_wrap_pre(void *wrapcxt, void **user_data) {
    /* Your pre-wrap code here */
    const char *func_name = (const char *) *user_data;
    dr_printf("Entering function: %s\n", func_name);
}

void generic_wrap_post(void *wrapcxt, void *user_data) {
    /* Your post-wrap code here */
    const char *func_name = (const char *) user_data;
    dr_printf("Exiting function: %s\n", func_name);
}

/* Callback function to filter and wrap symbols */
static bool symbol_filter(drsym_info_t *info, drsym_error_t status, void *data) {
    if (strcmp(info->name, "memcpy") == 0) {
        app_pc start = (app_pc)data; // Assuming data is the start address of the module
        app_pc func_pc = start + info->start_offs; // Correct pointer arithmetic
        // Wrap
        drwrap_wrap_ex(func_pc, generic_wrap_pre, generic_wrap_post, (void *)"memcpy", 0);
        dr_printf("Wrapped function: %s at address: %p\n", info->name, func_pc);
    }
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

/* Initialize the client */
DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {
    dr_set_client_name("DynamoRIO Sample Client", "http://dynamorio.org");

    drmgr_init();
    drwrap_init();
    if (drsym_init(0) != DRSYM_SUCCESS) {
        dr_printf("Failed to initialize symbol lookup\n");
        dr_exit_process(1);
    }

    /* Register event callbacks */
    drmgr_register_module_load_event(module_load_event);

    /* You may want to register other events here, such as module unload, thread init, etc. */
}

/* Cleanup function for the client */
DR_EXPORT void dr_client_exit(void) {
    drwrap_exit();
    drsym_exit();
    drmgr_exit();
}

