import subprocess
import sys
import os
import re
from pathlib import Path


def get_functions_from_binary(binary_path):
    """
    Uses 'nm' to extract all function names from the binary.

    Args:
        binary_path (str): The path to the binary file.

    Returns:
        set: A set of function names extracted from the binary.
    """
    try:
        # '-C' demangles C++ symbols, '--defined-only' lists only defined symbols
        nm_output = subprocess.check_output(['nm', '-C', '--defined-only', binary_path], text=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running nm: {e}")
        sys.exit(1)

    # Regular expression to match function symbols in the text section
    function_regex = re.compile(r'^[0-9a-fA-F]+\s+[tT]\s+(.+)$')

    functions_in_binary = set()

    for line in nm_output.splitlines():
        match = function_regex.match(line)
        if match:
            func_name = match.group(1).strip()
            functions_in_binary.add(func_name)

    return functions_in_binary


def get_functions_from_source(source_dirs):
    """
    Uses 'ctags' to extract user-defined function names from source files.

    Args:
        source_dirs (list): List of directories containing source code files.

    Returns:
        set: A set of function names extracted from the source code.
    """
    # Generate a temporary tags file
    tags_file = 'temp_tags'

    # Collect source files
    source_extensions = ['.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx']
    source_files = []

    for dir_path in source_dirs:
        dir_path = Path(dir_path)
        if not dir_path.is_dir():
            print(f"Warning: {dir_path} is not a directory.")
            continue
        for ext in source_extensions:
            source_files.extend(dir_path.rglob(f'*{ext}'))

    source_files = [str(file) for file in source_files]

    if not source_files:
        print("No source files found.")
        return set()

    # Run ctags to generate the tags file
    try:
        ctags_cmd = [
                        'ctags',
                        '-x',  # Output tags in a human-readable form
                        '--c-kinds=fp',  # Extract functions (f) and prototypes (p) in C
                        '--c++-kinds=fp',  # Extract functions (f) and prototypes (p) in C++
                        '--fields=+n',  # Include line numbers
                        '-o', '-',  # Output to stdout
                    ] + source_files

        ctags_output = subprocess.check_output(ctags_cmd, text=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running ctags: {e}")
        sys.exit(1)

    functions_in_source = set()

    for line in ctags_output.splitlines():
        parts = line.strip().split()
        if len(parts) >= 4:
            name = parts[0]
            kind = parts[1]
            if kind == 'function' or kind == 'prototype':
                functions_in_source.add(name)

    return functions_in_source


def main():
    if len(sys.argv) < 3:
        print("Usage: python get_user_functions.py <binary_path> <source_dir1> [<source_dir2> ...]")
        sys.exit(1)

    binary_path = sys.argv[1]
    source_dirs = sys.argv[2:]

    # Step 1: Extract functions from the binary using nm
    functions_in_binary = get_functions_from_binary(binary_path)
    print(f"Total functions in binary: {len(functions_in_binary)}")

    # Step 2: Extract user-defined functions from source files using ctags
    functions_in_source = get_functions_from_source(source_dirs)
    print(f"Total functions in source code: {len(functions_in_source)}")

    # Step 3: Take the intersection of functions from the binary and source code
    user_defined_functions = functions_in_binary.intersection(functions_in_source)
    print(f"Total user-defined functions in binary: {len(user_defined_functions)}")

    # Output the user-defined functions
    print("\nUser-defined functions present in the binary:")
    for func in sorted(user_defined_functions):
        print(func)


if __name__ == '__main__':
    main()
