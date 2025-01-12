import gdb
import re

# Regular expression to identify call instructions
CALL_INSTRUCTION_REGEX = re.compile(r'\bcall\b')
RETURN_INSTRUCTION_REGEX = re.compile(r'\bret\w*\b')

input_data = {
    "main": ["size1"],
    "processData": ["initialSize", "finalSize", "modifiedSize"],
    "riskyFunction": ["finalSize"],
    "performOverflow": ["smallBuffer", "largeBuffer", "size"],
    "combineData": ["size", "result"],
    "modifySize": ["size", "temp"],
    "memcpy": ["dest", "src", "size"],
}


def set_breakpoints(disasm, current_function_name):
    # Find and set breakpoints at all call instructions and return instructions
    call_instructions_number = 0
    return_instructions_number = 0
    breakpoints = []
    for line in disasm.splitlines():
        parts = line.strip().split()
        if len(parts) < 3:
            continue
        addr = parts[0]
        instr = parts[2]
        called_function_name = parts[-1].strip("<").strip(">").split("@")[0]


        if CALL_INSTRUCTION_REGEX.match(instr):
            print(line)
            addr_clean = addr.rstrip(':')
            #print(f"Found call instruction in main at {addr_clean}, function {called_function_name}")
            if called_function_name not in input_data:
                print(f"Function {called_function_name} not in input data.")
                continue
            BreakAtCallHandler(addr_clean)
            call_instructions_number += 1
            breakpoints.append(called_function_name)
        elif RETURN_INSTRUCTION_REGEX.match(instr):
            print(line)
            addr_clean = addr.rstrip(':')
            if current_function_name not in input_data:
                print(f"Function {current_function_name} not in input data.")
                continue
            BreakAtReturnHandler(addr_clean)
            return_instructions_number += 1
            breakpoints.append("ret")

    if call_instructions_number == 0 and return_instructions_number == 0:
        print("[End] No function calls or returns found.")
        return
    else:
        for i in range(call_instructions_number+return_instructions_number):
            # continue to the next breakpoint
            # when program exits
            try:
                gdb.execute("continue")
            except gdb.error as e:
                print(f"[End] Program exited with message: {e}")
                break
            step_into_next(breakpoints[i])



def step_into_next(breakpoint):
    """
    Step into the next function after stopping at the current function call.
    Then disassemble the new function.
    """
    # Automatically step into the next function call
    print(f"[Step] Stepping into the next  {breakpoint}")
    try:
        gdb.execute("step")
    except gdb.error as e:
        print(f"[End] Program exited with message: {e}")
        return
    if breakpoint != "ret":
        # Disassemble the new function after stepping in
        disasm = gdb.execute("disassemble", to_string=True)

        # Set breakpoints at the call instructions in the new function
        set_breakpoints(disasm, breakpoint)




def delete_main_breakpoints():
    """
    Deletes all breakpoints in the main function.
    """
    print("[Cleanup] Deleting all breakpoints in main function.")
    gdb.execute("delete breakpoints")


class BreakAtCallHandler(gdb.Breakpoint):
    """
    Class to handle breakpoints at call instructions. It will stop just before
    a function call and set a breakpoint at the function call.
    """

    def __init__(self, address):
        # break using to_string=True to avoid printing the message
        super(BreakAtCallHandler, self).__init__(f"*{address}", gdb.BP_BREAKPOINT, internal=True)
        self.address = address

    def stop(self):
        frame = gdb.selected_frame()

        print(f"\n[Breakpoint] Stopped at {self.address} before function call.")
        # print all of the local variables
        print("[Local Variables]:")
        block = frame.block()
        for symbol in block:
            print(f"{symbol.name} = {frame.read_var(symbol)}")

        print("[Global Variables]:")
        # print all of the global variables
        global_block = block
        while global_block and not global_block.is_global:
            global_block = global_block.superblock

        if global_block and global_block.is_global:
            global_symbols = [sym for sym in global_block if sym.is_variable and not sym.is_argument]
            for sym in global_symbols:
                try:
                    value = sym.value(frame)
                    print(f"  {sym.name} = {value}")
                except gdb.error:
                    print(f"  {sym.name} = <unavailable>")
        else:
            print("  <No global variables found or unable to access global block>")

        return True





class BreakAtReturnHandler(gdb.Breakpoint):
    """
    Class to handle breakpoints at return instructions. It will stop just before
    a function returns and set a breakpoint at the return instruction.
    """

    def __init__(self, address):
        super(BreakAtReturnHandler, self).__init__(f"*{address}", gdb.BP_BREAKPOINT, internal=True)
        self.address = address

    def stop(self):
        frame = gdb.selected_frame()

        print(f"\n[Breakpoint] Stopped at {self.address} before function return.")
        block = frame.block()
        for symbol in block:
            print(f"{symbol.name} = {frame.read_var(symbol)}")

        print("[Global Variables]:")
        # print all of the global variables
        global_block = block
        while global_block and not global_block.is_global:
            global_block = global_block.superblock

        if global_block and global_block.is_global:
            global_symbols = [sym for sym in global_block if sym.is_variable and not sym.is_argument]
            for sym in global_symbols:
                try:
                    value = sym.value(frame)
                    print(f"  {sym.name} = {value}")
                except gdb.error:
                    print(f"  {sym.name} = <unavailable>")
        else:
            print("  <No global variables found or unable to access global block>")
        return True


# Function to initialize the breakpoints at main
def initialize_at_main():
    # Break at main
    print("[Init] Breaking at main...")
    gdb.execute("break main", to_string=True)
    gdb.execute("run")
    current_function_name = "main"
    print(current_function_name)
    # Now we're at main, disassemble it
    disasm = gdb.execute("disassemble", to_string=True)
    set_breakpoints(disasm, current_function_name)





# Initialize the first step
initialize_at_main()


