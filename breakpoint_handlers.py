
import subprocess
import sys
from time import sleep

import gdb
import re
from abc import ABC, abstractmethod
import subprocess
import json
from format_value import *
from logging_set import *


"""
TYPE_CODE_ARRAY = 2
TYPE_CODE_BITSTRING = -1
TYPE_CODE_BOOL = 21
TYPE_CODE_CHAR = 20
TYPE_CODE_COMPLEX = 22
TYPE_CODE_DECFLOAT = 25
TYPE_CODE_ENUM = 5
TYPE_CODE_ERROR = 14
TYPE_CODE_FIXED_POINT = 29
TYPE_CODE_FLAGS = 6
TYPE_CODE_FLT = 9
TYPE_CODE_FUNC = 7
TYPE_CODE_INT = 8
TYPE_CODE_INTERNAL_FUNCTION = 27
TYPE_CODE_MEMBERPTR = 17
TYPE_CODE_METHOD = 15
TYPE_CODE_METHODPTR = 16
TYPE_CODE_MODULE = 26
TYPE_CODE_NAMELIST = 30
TYPE_CODE_NAMESPACE = 24
TYPE_CODE_PTR = 1
TYPE_CODE_RANGE = 12
TYPE_CODE_REF = 18
TYPE_CODE_RVALUE_REF = 19
TYPE_CODE_SET = 11
TYPE_CODE_STRING = 13
TYPE_CODE_STRUCT = 3
TYPE_CODE_TYPEDEF = 23
TYPE_CODE_UNION = 4
TYPE_CODE_VOID = 10
TYPE_CODE_XMETHOD = 28
"""



CALL_INSTRUCTION_REGEX = re.compile(r'\bcall\b')
RETURN_INSTRUCTION_REGEX = re.compile(r'\bret\b')
LEA_INSTRUCTION_REGEX = re.compile(r'\blea\b')


class DebuggerState:
    def __init__(self):
        self.function_data = {
            "breakpoints": []
        }
        self.should_continue = False
        # Tracks call counts: {caller: {callee: count}}
        self.call_counts = {}
        self.input_data = {}
        self.debugLevel = 0
        self.input_path = ""
        self.output_path = ""


debugger_state = DebuggerState()



CALL_INSTRUCTION_REGEX = re.compile(r'\bcall\b')
RETURN_INSTRUCTION_REGEX = re.compile(r'\bret\w*\b')
LEA_INSTRUCTION_REGEX = re.compile(r'\blea\b')

def demangle(mangled_name):
    result = subprocess.run(['c++filt', '--', mangled_name], stdout=subprocess.PIPE, text=True)
    return result.stdout.strip()


def step_into_next(breakpoint_type):
    """
    step into the next function, or step out of the current function
    in the next function, set breakpoints at call instructions and return instructions
    """
    try:
        gdb.execute("step", from_tty=False, to_string=True)
        logging.debug(f"Stepped into the next function.")
        if breakpoint_type != "ret":
            try:
                disasm = gdb.execute(f"disassemble {breakpoint_type}", to_string=True)
            except Exception as e:
                logging.error(f"Failed to disassemble {breakpoint_type}: {e}")
                return

            set_breakpoints(disasm, breakpoint_type)
    except Exception as e:
        logging.error(f"Failed to step into the next function: {e}")
    return


def delete_breakpoints():
    # if there are breakpoints no longer needed, delete them
    for bp in gdb.breakpoints():
        if bp.is_internal:
            bp.delete()


def post_callback_continue():
    try:
        gdb.execute("continue")
    except Exception as e:
        logging.error(f"Failed to continue: {e}")


def post_callback(breakpoint_type):
    try:
        gdb.post_event(lambda: post_callback_continue())
    except Exception as e:
        logging.error(f"Failed to schedule continue: {e}")
    return





class BreakpointHandler(ABC, gdb.Breakpoint):
    """
    Abstract base class for GDB breakpoint handlers.
    Defines the interface and shared functionality for all breakpoint handlers.
    """

    def __init__(self, address, function_name=None, caller_function=None):
        """
        Initializes the breakpoint handler.

        Args:
            address (str): The memory address or symbol where the breakpoint is set.
            function_name (str, optional): The name of the function associated with the breakpoint.
            caller_function (str, optional): The name of the caller function, if applicable.
        """
        # Ensure gdb.Breakpoint is the first base class for proper initialization
        super(BreakpointHandler, self).__init__(f"*{address}", gdb.BP_BREAKPOINT, internal=True)
        self.address = address
        self.function_name = function_name
        self.caller_function = caller_function

    @abstractmethod
    def stop(self):
        """
        Abstract method called when the breakpoint is hit.
        Must be implemented by all subclasses.
        """
        pass

    def is_variable_initialized(self, frame, symbol):
        """
        Determine if a variable is initialized by checking its memory status

        Args:
            frame: Current GDB frame
            symbol: Variable symbol to check

        Returns:
            bool: True if the variable is initialized
        """
        try:
            # Handle function parameters (always initialized when in function)
            if symbol.is_argument:
                return True

            # Try to read the variable's value
            value = frame.read_var(symbol)

            # Check if the variable has valid memory access
            try:
                # Get memory address of the variable
                addr = value.address

                # If we can access the memory and get a valid address,
                # the variable is likely initialized
                if addr is not None:
                    # Try to read the memory at this address
                    gdb.execute(f"x {addr}", to_string=True)
                    return True
            except:
                pass

            return False

        except gdb.MemoryError:
            print(f"Memory error reading {symbol.name}")
            return False
        except Exception as e:
            logging.debug(f"Error checking initialization status for {symbol.name}: {e}")
            return False


    def increment_call_count(self, increment=True):

        """
        Increments and retrieves the current call count for a specific function call.

        Returns:
            tuple: (current_count, total_times_called)
        """
        if self.caller_function:
            if self.caller_function not in debugger_state.input_data:
                return 0, 0
            if self.caller_function not in debugger_state.call_counts:
                debugger_state.call_counts[self.caller_function] = {}
            if self.function_name not in debugger_state.call_counts[self.caller_function]:
                debugger_state.call_counts[self.caller_function][self.function_name] = 0

            if increment:
                debugger_state.call_counts[self.caller_function][self.function_name] += 1
            current_count = debugger_state.call_counts[self.caller_function][self.function_name]

            total_times_called = 0
            if self.function_name in debugger_state.input_data[self.caller_function]['calls']:
                total_times_called = debugger_state.input_data[self.caller_function]['calls'][self.function_name]


            return current_count, total_times_called
        return 0, 0


    def collect_common_data(self, frame, state):
        """
        Collects common debugging data such as local variables, global variables, member variables, and arguments.

        Args:
            frame (gdb.Frame): The current GDB frame.

        Returns:
            dict: A dictionary containing the collected debugging data.
        """
        break_point = {
            "location": self.caller_function,
            "state": state,
            "local_vars": self.get_local_var(frame),
            "global_vars": self.get_global_var(frame),
            "member_vars": self.get_member_var(frame),
            "arguments": self.get_arguments(frame),
            "line": frame.find_sal().line
        }
        return break_point

    def get_local_var(self, frame):
        local_vars = {}
        try:
            block = frame.block()
        except Exception as e:
            logging.error(f"Failed to get frame block: {e}")
            return local_vars
        # print out all of the local variable names
        for symbol in block:
            # check if the symbol is initialized
            if not self.is_variable_initialized(frame, symbol):
                continue
            if symbol.is_variable:
                #print(symbol.name)
                value = frame.read_var(symbol)
                # use parse_and_eval to get value
                formatted_value = format_value(value, symbol.name)
                # str_value = str(formatted_value)
                # str_value = str_value.replace("\\000", "")
                #print(symbol.name, formatted_value)
                local_vars[symbol.name] = formatted_value

        return local_vars

    def get_global_var(self, frame):
        global_vars = {}
        try:
            global_block = frame.block()
        except Exception as e:
            logging.error(f"Failed to get frame block: {e}")
            return global_vars
        while global_block and not global_block.is_global:
            global_block = global_block.superblock

        if global_block and global_block.is_global:
            global_symbols = [sym for sym in global_block if sym.is_variable and not sym.is_argument]
            for symbol in global_symbols:
                if not self.is_variable_initialized(frame, symbol):
                    continue
                value = symbol.value(frame)
                formatted_value = format_value(value, symbol.name)
                # str_value = str(formatted_value)
                # str_value = str_value.replace("\\000", "")
                global_vars[symbol.name] = formatted_value

        else:
            logging.debug("  <No global variables found or unable to access global block>")
        return global_vars

    def get_member_var(self, frame):
        this_symbol = None
        try:
            block = frame.block()
        except Exception as e:
            logging.error(f"Failed to get frame block: {e}")
            return {}
        member_vars = {}
        while block:
            for symbol in block:
                # Look for the 'this' pointer which is typical in C++ member functions
                if symbol.name == 'this' and symbol.is_argument:
                    this_symbol = symbol
                    break
            if this_symbol:
                break
            block = block.superblock

        if this_symbol:
            this_value = frame.read_var(this_symbol)
            formatted_this_value = format_value(this_value, this_symbol.name)
            # str_value = str(formatted_this_value)
            # str_value = str_value.replace("\\000", "")
            member_vars["this"] = formatted_this_value

        return member_vars

    def get_arguments(self, frame):
        arguments = {}
        try:
            block = frame.block()
        except Exception as e:
            logging.error(f"Failed to get frame block: {e}")
            return arguments

        # Traverse the block hierarchy to find function arguments
        while block:
            for symbol in block:
                if not self.is_variable_initialized(frame, symbol):
                    continue
                if symbol.is_argument:  # Check if the symbol is an argument

                    arg_value = frame.read_var(symbol)
                    formatted_arg = format_value(arg_value)
                    # str_value = str(formatted_arg)
                    # str_value = str_value.replace("\\000", "")
                    arguments[symbol.name] = formatted_arg

            # Move up in the block hierarchy
            block = block.superblock
        return arguments

class BreakAtCallHandler(BreakpointHandler):

    def stop(self):
        logging.debug(f"Stopped at {self.function_name} at function start, called from {self.caller_function}.")
        current_count, total_times_called = self.increment_call_count()
        logging.info(f"Function Start: {self.function_name} called {current_count} times, total {total_times_called} times.")
        if current_count < total_times_called:
            # go continue
            gdb.post_event(lambda: post_callback(self.function_name))
            return True
        if current_count > total_times_called or current_count == 0:
            logging.error(f"Function {self.function_name} called more times than expected.")
            return False

        frame = gdb.selected_frame()
        sal = frame.find_sal()
        line_num = sal.line
        file_name = "unknown"
        try:
            file_name = sal.symtab.filename
        except Exception as e:
            pass
        logging.debug(f"SAL: {sal} Line: {line_num} File: {file_name}")

        break_point = self.collect_common_data(frame, "before function call of " + self.function_name)

        logging.info("[Local Variables]:")
        local_vars = self.get_local_var(frame)
        for key, value in local_vars.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["local_vars"] = local_vars


        logging.info("[Global Variables]:")
        global_vars = self.get_global_var(frame)
        for key, value in global_vars.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["global_vars"] = global_vars

        # Check for member variables if the current function has a 'this' pointer
        logging.info("[Member Variables]:")
        member_vars = self.get_member_var(frame)
        for key, value in member_vars.items():
            logging.info(f"  {key} = {json.dumps(value,indent=4)}")
        break_point["member_vars"] = member_vars

        # output arguments
        logging.info("[Arguments]:")
        arguments = self.get_arguments(frame)
        for key, value in arguments.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["arguments"] = arguments

        debugger_state.function_data["breakpoints"].append(break_point)
        # step into the next function
        gdb.post_event(lambda: post_callback(self.function_name))

        return True


class BreakAtFunctionStartHandler(BreakpointHandler):

    def stop(self):
        logging.debug(f"Stopped at {self.function_name} at function start, called from {self.caller_function}.")

        try:
            disasm = gdb.execute(f"disassemble {self.function_name}", to_string=True)
        except Exception as e:
            logging.error(f"Failed to disassemble {self.function_name}: {e}")
            return

        set_breakpoints(disasm, self.function_name, self.caller_function)

        # step into the next function
        gdb.post_event(lambda: post_callback(self.function_name))

        return True


class BreakAtReturnHandler(BreakpointHandler):

    def execute_continue(self):
        try:
            gdb.execute("continue")
            logging.debug("Continued")
        except Exception as e:
            logging.error(f"Failed to continue: {e}")
        return

    def stop(self):
        logging.debug(f"Stopped at {self.function_name} at function return, returning to {self.caller_function}.")
        if self.caller_function:
            current_count, total_times_called = self.increment_call_count(increment=False)
            logging.info(
                f"Function Return: {self.function_name} called {current_count} times, total {total_times_called} times.")
            if current_count < total_times_called:
                # go continue
                gdb.post_event(lambda: post_callback("ret"))
                return True

        frame = gdb.selected_frame()
        sal = frame.find_sal()
        line_num = sal.line
        file_name = "unknown"
        try:
            file_name = sal.symtab.filename
        except Exception as e:
            pass
        logging.debug(f"SAL: {sal} Line: {line_num} File: {file_name}")

        break_point = self.collect_common_data(frame, "before function return of " + self.function_name)

        # print all local variables
        logging.info("[Local Variables]:")
        local_vars = self.get_local_var(frame)
        for key, value in local_vars.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["local_vars"] = local_vars

        # print all global variables
        logging.info("[Global Variables]:")
        global_vars = self.get_global_var(frame)
        for key, value in global_vars.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["global_vars"] = global_vars

        # in cpp, we can also print out the member variables of the current object
        # if the current function is a member function
        # Check for member variables if the current function has a 'this' pointer
        logging.info("[Member Variables]:")
        member_vars = self.get_member_var(frame)
        for key, value in member_vars.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["member_vars"] = member_vars

        # output arguments
        logging.info("[Arguments]:")
        arguments = self.get_arguments(frame)
        for key, value in arguments.items():
            logging.info(f"  {key} = {json.dumps(value, indent=4)}")
        break_point["arguments"] = arguments

        debugger_state.function_data["breakpoints"].append(break_point)

        # step into the next function
        gdb.post_event(lambda: post_callback("ret"))

        return True


def set_breakpoints(disasm, current_function_name, caller_function_name=None):
    #logging.info(f"[Disassembly] {disasm}")
    call_instructions_number = 0
    function_start_instructions_number = 0
    return_instructions_number = 0
    breakpoints = []


    for line in disasm.splitlines():
        parts = line.strip().split()
        if len(parts) < 3:
            continue
        addr = parts[0]
        instr = parts[2]

        # edge case: call instruction with PLT
        match = re.search(r'call\s+.*@plt', line)

        # 0x0000555555557361 <+376>:   call   0x5555555570e0 <_Unwind_Resume@plt>
        if match:
            called_function_name = parts[-1].strip("<").strip(">").split("@")[0].strip("(").strip(")")
        else:
            called_function_name = parts[-1].strip("<").strip(">").strip("(").strip(")")
        called_function_addr = parts[-2]

        if CALL_INSTRUCTION_REGEX.search(instr) or LEA_INSTRUCTION_REGEX.search(instr):
            addr_clean = addr.rstrip(':')
            demangle_called_function_name = demangle(called_function_name)
            demangle_called_function_name_strip_args = demangle_called_function_name.split("(")[0]
            #print("INPUT debugger_state.input_data", debugger_state.input_data)
            if (called_function_name not in debugger_state.input_data and
                    demangle_called_function_name not in debugger_state.input_data and
                    demangle_called_function_name_strip_args not in debugger_state.input_data):
                #logging.info(f"Function {called_function_name} not found in input data.")
                #logging.info(f"Function {demangle_called_function_name} not found in input data.")

                continue
            called_function_name = demangle_called_function_name
            logging.info(f"[Call] {line}")
            # if there is not a breakpoint set at this address, set one
            if not any(bp.location == f"*{addr_clean}" for bp in gdb.breakpoints()):
                BreakAtCallHandler(addr_clean, called_function_name, current_function_name)
                call_instructions_number += 1
                breakpoints.append(called_function_name)

            # also break at the start of the function
            if not any(bp.location == f"*{called_function_addr}" for bp in gdb.breakpoints()):
                BreakAtFunctionStartHandler(called_function_addr, called_function_name, current_function_name)
                function_start_instructions_number += 1
                breakpoints.append(called_function_name)


        elif RETURN_INSTRUCTION_REGEX.search(instr):
            addr_clean = addr.rstrip(':')
            if current_function_name not in debugger_state.input_data:
                continue
            logging.info(f"[Return] {line}")

            # if there is not a breakpoint set at this address, set one
            if not any(bp.location == f"*{addr_clean}" for bp in gdb.breakpoints()):
                BreakAtReturnHandler(addr_clean, current_function_name, caller_function_name)
                return_instructions_number += 1
                breakpoints.append("ret")



