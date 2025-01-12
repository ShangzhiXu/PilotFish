/* **********************************************************
 * Copyright (c) 2012-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Code Manipulation API Sample:
 * instrcalls.c
 *
 * Instruments direct calls, indirect calls, and returns in the target
 * application.  For each dynamic execution, the call target and other
 * key information is written to a log file.  Note that this log file
 * can become quite large, and this client incurs more overhead than
 * the other clients due to its log file.
 *
 * If the SHOW_SYMBOLS define is on, this sample uses the drsyms
 * DynamoRIO Extension to obtain symbol information from raw
 * addresses.  This requires a relatively recent copy of dbghelp.dll
 * (6.0+), which is not available in the system directory by default
 * on Windows 2000.  To use this sample with SHOW_SYMBOLS on Windows
 * 2000, download the Debugging Tools for Windows package from
 * http://www.microsoft.com/whdc/devtools/debugging/default.mspx and
 * place dbghelp.dll in the same directory as either drsyms.dll or as
 * this sample client library.
 */

#include "dr_api.h"
#include "drmgr.h"
#include <libdwarf.h>
#include <dwarf.h>
#include "drsyms.h"
#include <cstring>
#include "utils.h"
//set SHOW_SYMBOLS to 1 to show symbol information
#define SHOW_SYMBOLS 1
static void
event_exit(void);
static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data);
static int tls_idx;



static app_pc my_module_start = NULL;
static app_pc my_module_end = NULL;
static client_id_t my_id;
static drsym_info_t sym_info;
static module_data_t *mod_data = NULL;
static Dwarf_Debug dbg = NULL;

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'instrcalls'",
                       "http://dynamorio.org/issues");
    drmgr_init();
    my_id = id;
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'instrcalls' initializing\n");
    /* also give notification to stderr */
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client instrcalls is running\n");
    }
#endif
    dr_register_exit_event(event_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL);
    //drmgr_register_thread_init_event(event_thread_init);
    //drmgr_register_thread_exit_event(event_thread_exit);
    dr_printf("Client 'instrcalls' is ready to instrument\n");
#ifdef SHOW_SYMBOLS
    if (drsym_init(0) != DRSYM_SUCCESS) {
        dr_log(NULL, DR_LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n");
    }
#endif

    // get the module start and size
    module_data_t *data = dr_get_main_module();
    my_module_start = data->start;
    my_module_end = data->end;
    dr_free_module_data(data);

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx > -1);
}

static void
event_exit(void)
{
#ifdef SHOW_SYMBOLS
    if (drsym_exit() != DRSYM_SUCCESS) {
        dr_log(NULL, DR_LOG_ALL, 1, "WARNING: error cleaning up symbol library\n");
    }
#endif
    drmgr_unregister_tls_field(tls_idx);
    drmgr_exit();
}

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x) /* nothing */
#endif


// Function to get the current stack pointer and print the value of a variable
void print_variable_value(void *drcontext, const char *var_name) {
    module_data_t *mod_data;
    drsym_error_t res;
    drsym_info_t sym_info;
    size_t offset;
    app_pc base_address;

    sym_info.struct_size = sizeof(sym_info);

    // Iterate through loaded modules
    dr_module_iterator_t *mod_iter = dr_module_iterator_start();
    while (dr_module_iterator_hasnext(mod_iter)) {
        mod_data = dr_module_iterator_next(mod_iter);

        // Lookup the symbol (variable name) in the module's symbol table
        res = drsym_lookup_symbol(mod_data->full_path, var_name, &offset, DRSYM_DEFAULT_FLAGS);
        if (res == DRSYM_SUCCESS) {
            // Found the symbol in this module, calculate its address
            base_address = (app_pc)mod_data->start + offset;

            // Get the current stack pointer and context
            dr_mcontext_t mc = { sizeof(mc), DR_MC_ALL };
            dr_get_mcontext(drcontext, &mc);

            // Assuming the variable is an integer on the stack
            int *variable_address = (int *)base_address;

            // Print the variable's value
            dr_printf("Variable '%s' found at address %p, value: %d\n", var_name, (void *)variable_address, *variable_address);
        } else {
            dr_printf("Variable '%s' not found in module '%s'.\n", var_name, mod_data->full_path);
        }
    }
    dr_module_iterator_stop(mod_iter);
}


app_pc resolve_variable_address(const char *var_name) {
    size_t symbol_offset = 0;
    drsym_error_t res;

    // Iterate through loaded modules to find the variable
    app_pc variable_addr = NULL;
    dr_module_iterator_t *mod_iter = dr_module_iterator_start();

    while (dr_module_iterator_hasnext(mod_iter)) {
        mod_data = dr_module_iterator_next(mod_iter);

        // Look up the symbol (variable) in the current module
        res = drsym_lookup_symbol(mod_data->full_path, var_name, &symbol_offset, DRSYM_DEFAULT_FLAGS);

        if (res == DRSYM_SUCCESS) {
            // Calculate the address of the variable
            variable_addr = mod_data->start + symbol_offset;
            break;
        }
    }

    dr_module_iterator_stop(mod_iter);

    if (variable_addr == NULL) {
        dr_printf("Variable %s not found\n", var_name);
    }

    return variable_addr;
}

void print_variable_value(const char *var_name) {
    app_pc var_addr = resolve_variable_address(var_name);

    if (var_addr != NULL) {
        // Assuming the variable is an int for simplicity
        int value = *(int *)var_addr;  // Adjust type depending on variable type
        dr_printf("Value of variable %s: %d\n", var_name, value);
    }
}



static void
event_thread_init(void *drcontext)
{
    file_t f;
    /* We're going to dump our data to a per-thread file.
     * On Windows we need an absolute path so we place it in
     * the same directory as our library. We could also pass
     * in a path as a client argument.
     */
    f = log_file_open(my_id, drcontext, NULL /* client lib path */, "instrcalls",
#ifndef WINDOWS
                      DR_FILE_CLOSE_ON_FORK |
                      #endif
                      DR_FILE_ALLOW_LARGE);
    DR_ASSERT(f != INVALID_FILE);

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, (void *)(ptr_uint_t)f);
}

static void
event_thread_exit(void *drcontext)
{
    log_file_close((file_t)(ptr_uint_t)drmgr_get_tls_field(drcontext, tls_idx));
}

#ifdef SHOW_SYMBOLS
#    define MAX_SYM_RESULT 256
static void
print_address(file_t f, app_pc addr, const char *prefix)
{
    drsym_error_t symres;
    drsym_info_t sym;
    char name[MAX_SYM_RESULT];
    char file[MAXIMUM_PATH];
    module_data_t *data;
    data = dr_lookup_module(addr);
    if (data == NULL) {
        dr_fprintf(f, "%s " PFX " ? ??:0\n", prefix, addr);
        return;
    }
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;
    symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
                                  DRSYM_DEFAULT_FLAGS);
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        const char *modname = dr_module_preferred_name(data);
        if (modname == NULL)
            modname = "<noname>";
        dr_fprintf(f, "%s " PFX " %s!%s+" PIFX, prefix, addr, modname, sym.name,
                   addr - data->start - sym.start_offs);
        if (symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
            dr_fprintf(f, " ??:0\n");
        } else {
            dr_fprintf(f, " %s:%" UINT64_FORMAT_CODE "+" PIFX "\n", sym.file, sym.line,
                       sym.line_offs);
        }
    } else
        dr_fprintf(f, "%s " PFX " ? ??:0\n", prefix, addr);
    dr_free_module_data(data);
}
#endif

static void
at_call(app_pc instr_addr, app_pc target_addr)
{
    file_t f =
    (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
#ifdef SHOW_SYMBOLS
    //print_address(f, instr_addr, "CALL @ ");
    //print_address(f, target_addr, "\t to ");
    //dr_fprintf(f, "\tTOS is " PFX "\n", mc.xsp);
#else
    //dr_fprintf(f, "CALL @ " PFX " to " PFX ", TOS is " PFX "\n", instr_addr, target_addr,
            mc.xsp);
#endif
}

static void
at_call_ind(app_pc instr_addr, app_pc target_addr)
{
    file_t f =
    (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
#ifdef SHOW_SYMBOLS
    //print_address(f, instr_addr, "CALL INDIRECT @ ");
    //print_address(f, target_addr, "\t to ");
#else
    //dr_fprintf(f, "CALL INDIRECT @ " PFX " to " PFX "\n", instr_addr, target_addr);
#endif
}

static void
at_return(app_pc instr_addr, app_pc target_addr)
{
    file_t f =
    (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
#ifdef SHOW_SYMBOLS
    //print_address(f, instr_addr, "RETURN @ ");
    //print_address(f, target_addr, "\t to ");
#else
    //dr_fprintf(f, "RETURN @ " PFX " to " PFX "\n", instr_addr, target_addr);
#endif
}

// Function to extract and print variable value using DWARF information
void print_variable_value_with_dwarf(void *drcontext)
{
    char *var_name = "size";
    Dwarf_Error err;
    Dwarf_Die cu_die = nullptr;
    Dwarf_Unsigned cu_length = 0;
    Dwarf_Addr var_pc;
    Dwarf_Die child_die = nullptr;
    Dwarf_Half tag;
    Dwarf_Attribute loc_attr;
    dr_mcontext_t mc = { sizeof(mc), DR_MC_ALL };

    // Get CPU context to access the registers (for base pointer, etc.)
    dr_get_mcontext(drcontext, &mc);

    // Iterate over all Compilation Units (CUs)
    while (dwarf_next_cu_header_b(dbg, &cu_length, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &err) == DW_DLV_OK) {

        if (dwarf_siblingof(dbg, nullptr, &cu_die, &err) != DW_DLV_OK) {
            dr_printf("Error retrieving sibling CU DIE: %s\n", dwarf_errmsg(err));
            continue;
        }

        // Iterate over DIEs in the CU to find the variable
        Dwarf_Die current_die = cu_die;
        while (dwarf_siblingof(dbg, current_die, &current_die, &err) == DW_DLV_OK) {
            if (dwarf_tag(current_die, &tag, &err) != DW_DLV_OK) {
                continue;
            }

            if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
                char *die_name = nullptr;
                if (dwarf_diename(current_die, &die_name, &err) == DW_DLV_OK) {
                    if (strcmp(die_name, var_name) == 0) {
                        dr_printf("Found variable: %s\n", die_name);

                        // Found the variable, now get the location
                        if (dwarf_attr(current_die, DW_AT_location, &loc_attr, &err) == DW_DLV_OK) {
                            Dwarf_Locdesc **locdesc;
                            Dwarf_Signed numloc;
                            if (dwarf_loclist_n(loc_attr, &locdesc, &numloc, &err) == DW_DLV_OK) {
                                for (int i = 0; i < numloc; i++) {
                                    Dwarf_Locdesc *loc = locdesc[i];
                                    if (loc != NULL && loc->ld_cents > 0) {
                                        // Handling different types of DWARF location expressions
                                        for (int j = 0; j < loc->ld_cents; j++) {
                                            Dwarf_Loc *location_expr = &loc->ld_s[j];

                                            if (location_expr->lr_atom == DW_OP_fbreg) {
                                                // Variable stored on the stack, calculate the address
                                                var_pc = mc.rbp + location_expr->lr_number;
                                                dr_printf("Variable '%s' at stack address %p with value: %d\n", var_name, (void*)var_pc, *(int *)var_pc);
                                                dwarf_dealloc(dbg, locdesc, DW_DLA_LOCDESC);
                                                return;
                                            } else if (location_expr->lr_atom >= DW_OP_reg0 && location_expr->lr_atom <= DW_OP_reg15) {
                                                // Variable stored in a register, map register number to context fields
                                                int reg_value;
                                                switch (location_expr->lr_atom - DW_OP_reg0) {
                                                    case 0: reg_value = mc.xax; break;
                                                    case 1: reg_value = mc.xdx; break;
                                                    case 2: reg_value = mc.xcx; break;
                                                    case 3: reg_value = mc.xbx; break;
                                                    case 4: reg_value = mc.xsp; break;
                                                    case 5: reg_value = mc.xbp; break;
                                                    case 6: reg_value = mc.xsi; break;
                                                    case 7: reg_value = mc.xdi; break;
                                                    default: dr_printf("Unsupported register!\n"); continue;
                                                }
                                                dr_printf("Variable '%s' found in register with value: %d\n", var_name, reg_value);
                                                dwarf_dealloc(dbg, locdesc, DW_DLA_LOCDESC);
                                                return;
                                            }
                                        }
                                    }
                                }
                                dwarf_dealloc(dbg, locdesc, DW_DLA_LOCDESC);
                            }
                        }
                    }
                }
            }
        }
    }

    dr_printf("Variable '%s' not found!\n", var_name);
}


static void print_instr_and_source(void *drcontext, app_pc pc)
{
    if (pc == nullptr) {
        return;
    }
    if (pc < my_module_start || pc >= my_module_end) {
        return;
    }

    // Buffer for disassembled instruction
    char disassembled_instr[256];

    // Get the current module data
    module_data_t *mod_data = dr_lookup_module(pc);
    if (mod_data == NULL) {
        dr_printf("No module found for PC: %p\n", pc);
        return;
    }

    // Prepare to retrieve symbol information
    drsym_info_t sym_info;
    char file_buf[256];
    char name_buf[256];

    sym_info.struct_size = sizeof(sym_info);
    sym_info.name = name_buf;
    sym_info.name_size = sizeof(name_buf);
    sym_info.file = file_buf;
    sym_info.file_size = sizeof(file_buf);

    // Resolve symbol information (source file, line number)
    drsym_error_t res = drsym_lookup_address(mod_data->full_path, pc - mod_data->start, &sym_info, DRSYM_DEFAULT_FLAGS);


    if (res == DRSYM_SUCCESS) {
        dr_printf("Instruction at %p in %s:%d\n", pc, sym_info.file, sym_info.line);
        if (sym_info.line == 150) {
            print_variable_value_with_dwarf(drcontext);
        }
    }

    dr_free_module_data(mod_data);
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{

#ifdef VERBOSE
    if (drmgr_is_first_instr(drcontext, instr)) {
        dr_printf("in dr_basic_block(tag=" PFX ")\n", tag);
#    if VERBOSE_VERBOSE
        instrlist_disassemble(drcontext, tag, bb, STDOUT);
#    endif
    }
#endif

    /* instrument calls and returns -- ignore far calls/rets */
    //output instr
    app_pc pc = instr_get_app_pc(instr);
    if (pc == nullptr) {
        return DR_EMIT_DEFAULT;
    }
    // use instr_disassemble() to get the disassembled instruction
    char buffer[256];
    instr_disassemble_to_buffer(drcontext, instr, buffer, sizeof(buffer));


    print_instr_and_source(drcontext, pc);

    if (instr_is_call_direct(instr)) {
        dr_insert_call_instrumentation(drcontext, bb, instr, (app_pc)at_call);
    } else if (instr_is_call_indirect(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)at_call_ind,
                                      SPILL_SLOT_1);
    } else if (instr_is_return(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)at_return,
                                      SPILL_SLOT_1);
    }
    return DR_EMIT_DEFAULT;
}