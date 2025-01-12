

import sys
import logging
import json
import io
import sys
import gdb



# Create handlers
file_handler = logging.FileHandler('debugger.log', mode='w')
file_handler.setLevel(logging.DEBUG)

console_handler = logging.StreamHandler(sys.stdout)
console_handler.setLevel(logging.INFO)

# Create formatters
file_formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
console_formatter = logging.Formatter('%(levelname)s - %(message)s')

# Add formatters to handlers
file_handler.setFormatter(file_formatter)
console_handler.setFormatter(console_formatter)

# Get the root logger and set level
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

# Add handlers to the logger
logger.addHandler(file_handler)
logger.addHandler(console_handler)


def format_captured_output(output: str) -> str:
    inside_braces = False
    formatted_output = ""

    for char in output:
        if char == "{":
            inside_braces = True
            formatted_output += char  # Keep the opening brace
        elif char == "}":
            inside_braces = False
            formatted_output += char  # Keep the closing brace
        elif inside_braces:
            if char != '\n' and char != ' ':
                formatted_output += char  # Remove newlines and spaces inside braces
        else:
            formatted_output += char  # Keep content outside braces

    return formatted_output


def pretty_print_container(container_type, value, symbol_name):
    if symbol_name is None:
        return
    try:
        # Use gdb.execute() to capture the output directly
        captured_output = gdb.execute(f"print {symbol_name}", to_string=True)

        # Format the captured output
        formatted_output = format_captured_output(captured_output)

        # Print the results
        return formatted_output
    except Exception as e:
        print(f"Error while pretty printing: {e}")
