


import gdb
import os
import sys
current_dir = os.path.dirname(os.path.abspath(__file__))
print(f"Current directory: {current_dir}")
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)
print(sys.path)

from breakpoint_handlers import *


def on_exit_handler(event):
    try:
        with open(debugger_state.output_path, 'w') as f:
            json.dump(debugger_state.function_data, f, ensure_ascii=False, indent=4)
    except Exception as e:
        logging.error(f"Failed to write output data: {e}")


# utility functions
def set_gdb_print_options():
    try:
        gdb.execute("set python print-stack full", to_string=True)
        gdb.execute("set print repeats unlimited", to_string=True)
        gdb.execute("set print array on", to_string=True)
        gdb.execute("set pagination off")  # Disable pagination to simplify output
    except Exception as e:
        logging.error(f"Failed to set GDB print options: {e}")



def load_input_data(json_file_path):
    """
    Loads input data from a JSON file.

    Args:
        json_file_path (str): The path to the JSON file containing input data.

    Returns:
        dict: The parsed input data as a Python dictionary.
    """
    try:
        with open(json_file_path, 'r') as f:
            input_data = json.load(f)
        return input_data
    except FileNotFoundError:
        logging.error(f"Input JSON file not found: {json_file_path}")
        raise
    except json.JSONDecodeError as e:
        logging.error(f"JSON decoding error in {json_file_path}: {e}")
        raise
    except Exception as e:
        logging.error(f"Unexpected error loading input data: {e}")
        raise


def process_input_data(input_data):
    """
    Processes the input_data to map calls to times_called for easier access.

    Args:
        input_data (dict): The raw input_data loaded from JSON.

    Returns:
        dict: Processed input_data with calls mapped to times_called.
    """
    processed_data = {}
    for func, details in input_data.items():
        calls = details.get('calls', [])
        times_called = details.get('times_called', [])
        # Map each call to its corresponding times_called
        call_times_map = {}
        for i, call in enumerate(calls):
            if i < len(times_called):
                call_times_map[call] = times_called[i]
            else:
                call_times_map[call] = 1  # Default to 1 if not specified
        processed_data[func] = {
            'local_vars': details.get('local_vars', []),
            'calls': call_times_map
        }
    return processed_data


def load_config(config_file_path="config.json"):
    """
    Loads configuration data from a JSON file.

    Args:
        config_file_path (str): The path to the configuration JSON file.

    Returns:
        dict: The parsed configuration data as a Python dictionary.
    """
    try:
        with open(config_file_path, 'r') as f:
            config_data = json.load(f)
        return config_data
    except FileNotFoundError:
        logging.error(f"Configuration JSON file not found: {config_file_path}")
        raise
    except json.JSONDecodeError as e:
        logging.error(f"JSON decoding error in {config_file_path}: {e}")
        raise
    except Exception as e:
        logging.error(f"Unexpected error loading configuration data: {e}")
        raise

def initialize():
    global debug
    global debug_break
    global debug_disasm

    config_data = load_config()
    print("Config data:", config_data)
    debugger_state.input_path = config_data.get("input", "input.json")
    debugger_state.stdinput_path = config_data.get("stdinput", "input.txt")
    debugger_state.output_path = config_data.get("output", "output.json")
    debugger_state.debugLevel = config_data.get("debugLevel", 0)
    debug = config_data.get("debug", False)
    debug_break = config_data.get("debug_break", False)
    debug_disasm = config_data.get("debug_disasm", False)
    args = config_data.get("args", [])

    set_gdb_print_options()
    debugger_state.input_data = process_input_data(load_input_data(debugger_state.input_path))
    gdb.events.exited.connect(on_exit_handler)

    try:
        gdb.execute("break _start", to_string=True)
        if args:
            args_str = " ".join(args)
            run_cmd = f"run {args_str} < {debugger_state.stdinput_path}"
        else:
            run_cmd = f"run < {debugger_state.stdinput_path}"
        gdb.execute(run_cmd, to_string=True)


    except Exception as e:
        logging.error(f"Failed to set breakpoints and run the program: {e}")
        return

    # set breakpoints
    disasm = gdb.execute("disassemble main", to_string=True)
    first_instruction_address = disasm.splitlines()[1].split()[0]  # Extract address
    gdb.execute(f"break *{first_instruction_address}", to_string=True)
    print("Breakpoint set at first instruction of main at address:", first_instruction_address)
    gdb.execute("continue")
    print("Program started. Waiting for breakpoints...")
    debugger_state.call_counts["_start"] = {}
    debugger_state.call_counts["_start"]["main"] = 1
    set_breakpoints(disasm, "main", "_start")
    # continue to the next breakpoint
    #gdb.execute("continue")



initialize()
