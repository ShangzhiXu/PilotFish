#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_obj.h"

#include <libdwarf.h>  // Link libdwarf during compilation
#include <dwarf.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <fcntl.h>  // For open()
#include <unistd.h> // For close()
#include <cstring>

using namespace std;
typedef struct {
    reg_id_t reg_addr;
    int last_opcode;
} instru_data_t;
// Global variable to hold the DWARF debug info
static Dwarf_Debug dbg = nullptr;

// Function to initialize DWARF debugging
bool initialize_dwarf(const char *binary_path) {
    cout << "Initializing DWARF debug information for binary: " << binary_path << endl;
    // Open the binary file in read-only mode
    int fd = open(binary_path, O_RDONLY);
    if (fd == -1) {
        dr_printf("Failed to open the binary file: %s\n", binary_path);
        return false;
    }

    Dwarf_Error err;
    int res = dwarf_init(fd, DW_DLC_READ, nullptr, nullptr, &dbg, &err);
    if (res != DW_DLV_OK) {
        dr_printf("Failed to initialize DWARF debug information.\n");
        close(fd);
        return false;
    }
    cout << "DWARF debug information initialized successfully." << endl;
    // File descriptor will be closed after processing DWARF
    return true;
}

// Function to retrieve lines for a compilation unit (CU)
static size_t get_lines_from_cu(Dwarf_Die cu_die, Dwarf_Line **lines_out, Dwarf_Error *err) {
    Dwarf_Line *lines;
    Dwarf_Signed num_lines;

    // Retrieve source lines associated with the CU using dwarf_srclines
    if (dwarf_srclines(cu_die, &lines, &num_lines, err) != DW_DLV_OK) {
        dr_printf("Error retrieving source lines: %s\n", dwarf_errmsg(*err));
        return -1;
    }

    *lines_out = lines; // Return the lines
    return num_lines;   // Return the number of lines
}

// Function to map program counter (PC) to source code line
void output_source_code_from_pc(app_pc pc) {

    dr_printf("Looking for PC: %p\n", pc);

    Dwarf_Error err;
    Dwarf_Die cu_die = nullptr;
    Dwarf_Unsigned cu_length = 0;
    Dwarf_Unsigned line_no;
    char *file_name = nullptr;

    // Try to get the first compilation unit (CU)
//    int res = dwarf_next_cu_header_b(dbg, &cu_length, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &err);
//    if (res == DW_DLV_NO_ENTRY) {
//        dr_printf("No more CUs available.\n");
//        return;
//    } else if (res != DW_DLV_OK) {
//        dr_printf("Error reading CU: %s\n", dwarf_errmsg(err));
//        return;
//    } else if (res == DW_DLV_OK){
//        dr_printf("Successfully found a CU.\n");
//    }

    // Iterate through compilation units (CUs) to find the corresponding DIE
    while (dwarf_next_cu_header_b(dbg, &cu_length, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &err) == DW_DLV_OK) {
        if (dwarf_siblingof(dbg, nullptr, &cu_die, &err) == DW_DLV_OK) {


            Dwarf_Signed linecount = 0;
            Dwarf_Line *linebuf = nullptr;
            char *die_name = nullptr;
            char *func_name = nullptr;
            bool found_var = false;
            // Retrieve the name of the current DIE (Debugging Information Entry)
            if (dwarf_diename(cu_die, &die_name, &err) == DW_DLV_OK) {
                //print file name
                dr_printf("Checking file: %s\n", die_name);
                //further print function name
            } else {
                dr_printf("Error retrieving file name: %s\n", dwarf_errmsg(err));
            }



            // Get the lines associated with the current CU
            Dwarf_Signed num_lines = get_lines_from_cu(cu_die, &linebuf, &err);
            if (num_lines <= 0) {
                dr_printf("No lines found for this CU.\n");
                continue;
            }

            dr_printf("Found %ld lines in the CU.\n", num_lines);
            for (size_t i = 0; i < num_lines; i++) {
                Dwarf_Addr line_pc;
                if (dwarf_lineaddr(linebuf[i], &line_pc, &err) == DW_DLV_OK) {
                    // Compare the program counter with the line's address
                    if (line_pc == (Dwarf_Addr)pc) {
                        Dwarf_Unsigned line_no;
                        char *file_name = nullptr;

                        if (dwarf_lineno(linebuf[i], &line_no, &err) == DW_DLV_OK &&
                            dwarf_linesrc(linebuf[i], &file_name, &err) == DW_DLV_OK) {
                            dr_printf("Executed Source Code: %s:%llu\n", file_name, line_no);
                        }
                    }
                }
            }

            if (dwarf_siblingof(dbg, nullptr, &cu_die, &err) == DW_DLV_OK) {
                Dwarf_Die child_die;    // Child DIE (for variables, functions, etc.)

                // Iterate over child DIEs within the CU to find the variable DIE
                if (dwarf_child(cu_die, &child_die, &err) == DW_DLV_OK) {
                    do {
                        char *die_name = nullptr;

                        // Get the name of the DIE (variable or other entity)
                        if (dwarf_diename(child_die, &die_name, &err) == DW_DLV_OK) {
                            // Check if this DIE matches the variable name we're looking for
                            char *var_name = "size";
                            if (strcmp(die_name, var_name) == 0) {
                                found_var = true;
                                dr_printf("Variable '%s' found in CU.\n", var_name);

                                // Retrieve the variable's location (DW_AT_location)
                                Dwarf_Attribute loc_attr;
                                if (dwarf_attr(child_die, DW_AT_location, &loc_attr, &err) == DW_DLV_OK) {
                                    Dwarf_Locdesc **locdesc = nullptr;
                                    Dwarf_Signed numloc;

                                    // Get the location list (describing where the variable is stored)
                                    if (dwarf_loclist_n(loc_attr, &locdesc, &numloc, &err) == DW_DLV_OK) {
                                        // Handle stack-based variables (DW_OP_fbreg represents stack locations)
                                        if (locdesc[0]->ld_s->lr_atom == DW_OP_fbreg) {
                                            dr_printf("Variable '%s' is stored on the stack.\n", var_name);

                                            // Get the current stack pointer (SP) at runtime
                                            dr_mcontext_t mc;
                                            mc.size = sizeof(mc);
                                            mc.flags = DR_MC_ALL;
                                            dr_get_mcontext(dr_get_current_drcontext(), &mc);

                                            // Calculate the variable's address on the stack using the offset
                                            Dwarf_Signed offset = locdesc[0]->ld_s->lr_number;
                                            void* var_address = (void*)((char*)mc.xsp + offset);

                                            // Print the value of the variable (assuming it's an int for simplicity)
                                            int value;
                                            memcpy(&value, var_address, sizeof(int));
                                            dr_printf("Value of variable '%s': %d\n", var_name, value);
                                        } else {
                                            dr_printf("Unhandled location type for variable '%s'.\n", var_name);
                                        }
                                    } else {
                                        dr_printf("Error retrieving location list for variable '%s': %s\n", var_name,
                                                  dwarf_errmsg(err));
                                    }
                                } else {
                                    dr_printf("Error retrieving location attribute for variable '%s': %s\n", var_name,
                                              dwarf_errmsg(err));
                                }
                            }
                        }
                    } while (dwarf_siblingof(dbg, child_die, &child_die, &err) ==
                             DW_DLV_OK); // Continue to next sibling DIE
                }
            }



        } else {
            dr_printf("Error retrieving sibling CU DIE: %s\n", dwarf_errmsg(err));
        }
    }

}



// Instrumentation callback for each instruction
static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace, bool translating, void *user_data) {
    // Get the program counter (PC) for the current instruction
    instr_t *instr_fetch = drmgr_orig_app_instr_for_fetch(drcontext);
    instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
    //output
    // Get the program counter (PC) for the current instruction
    app_pc pc = instr_get_app_pc(instr);
    if (pc == nullptr) {
        return DR_EMIT_DEFAULT;
    }

    // Output the disassembled instruction to console
    if (instr_writes_memory(instr_operands)) {
        char buffer[256];
        instr_disassemble_to_buffer(drcontext, instr, buffer, sizeof(buffer));
        dr_printf("Instruction: %s\n", buffer);
        // Print the source code information for the instruction
        output_source_code_from_pc(pc);
    }



    return DR_EMIT_DEFAULT;
}

// Client initialization
DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {
    // Initialize DWARF debug information for the binary

    if (!initialize_dwarf("./buffer_overflow")) {
        dr_printf("Failed to initialize DWARF.\n");
        return;
    }

    // Initialize DynamoRIO manager
    drmgr_init();

    // Register the basic block (BB) instrumentation callback
    drmgr_register_bb_instrumentation_event(nullptr, event_app_instruction, nullptr);

    // Register an exit event to clean up
    dr_register_exit_event([] {
        // Clean up DWARF resources
        Dwarf_Error err;
        dwarf_finish(dbg, &err);

        // Clean up DynamoRIO resources
        drmgr_exit();
    });
}