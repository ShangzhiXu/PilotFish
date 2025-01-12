import sys
import os
import json
import re
import subprocess
from pathlib import Path

# List of source code file extensions to consider
SOURCE_EXTENSIONS = ['.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx']

def extract_defines_from_file(filename):
    defines = {}
    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        in_define = False
        current_define = ''
        for line in f:
            stripped_line = line.lstrip()
            if not in_define:
                if stripped_line.startswith('#define'):
                    current_define = stripped_line.rstrip('\n')
                    if ends_with_backslash(line):
                        in_define = True
                    else:
                        key, value = parse_define(current_define)
                        if key:
                            defines.setdefault(key, set()).add(value)
                    current_define = ''
                else:
                    continue
            else:
                current_define += '\n' + stripped_line.rstrip('\n')
                if ends_with_backslash(line):
                    continue
                else:
                    key, value = parse_define(current_define)
                    if key:
                        defines.setdefault(key, set()).add(value)
                    current_define = ''
                    in_define = False
    return defines

def ends_with_backslash(line):
    stripped_line = line.rstrip('\n').rstrip()
    return stripped_line.endswith('\\')

def extract_defines_from_directory(directory):
    all_defines = {}
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.lower().endswith(tuple(SOURCE_EXTENSIONS)):
                file_path = os.path.join(root, file)
                defines = extract_defines_from_file(file_path)
                for key, values in defines.items():
                    if key in all_defines:
                        all_defines[key].update(values)
                    else:
                        all_defines[key] = values
    return all_defines

def parse_define(define_line):
    # Use regex to correctly parse the macro name and body
    match = re.match(r'#define\s+(\w+(\(.*?\))?)\s*(.*)', define_line, re.DOTALL)
    if match:
        macro_name = match.group(1)
        macro_body = match.group(3)
        return macro_name, macro_body
    else:
        return None, None

def get_functions_from_binary(binary_path):
    """
    Uses 'nm' to extract all function names from the binary.
    """
    try:
        # '-C' demangles C++ symbols, '--defined-only' lists only defined symbols
        nm_output = subprocess.check_output(['nm', '-C', '--defined-only', binary_path], text=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running nm on {binary_path}: {e}")
        sys.exit(1)

    # Regular expression to match function symbols in the text section
    function_regex = re.compile(r'^[0-9a-fA-F]+\s+[tTwW]\s+(.+)$')

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
    """
    # Collect source files
    source_files = []

    for dir_path in source_dirs:
        dir_path = Path(dir_path)
        if not dir_path.is_dir():
            print(f"Warning: {dir_path} is not a directory.")
            continue
        for ext in SOURCE_EXTENSIONS:
            source_files.extend(dir_path.rglob(f'*{ext}'))

    source_files = [str(file) for file in source_files]

    if not source_files:
        print("No source files found.")
        return set()

    # Run ctags to generate the tags output
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
    except FileNotFoundError:
        print("Error: 'ctags' command not found. Please install ctags.")
        sys.exit(1)

    functions_in_source = set()

    for line in ctags_output.splitlines():
        parts = line.strip().split()
        if len(parts) >= 2:
            name = parts[0]
            kind = parts[1]
            if kind == 'function' or kind == 'prototype':
                functions_in_source.add(name)

    return functions_in_source


def strip_cpp_function_signature(func_name):
    """
    Strips the C++ function signature to get the base function name.
    Example: "AP4_MpegAudioSampleEntry::AP4_MpegAudioSampleEntry(unsigned int, unsigned int)"
            -> "AP4_MpegAudioSampleEntry"
    """
    # Remove everything after and including the first '('
    base_name = func_name.split('(')[0]
    # Get the part after the last '::'
    if '::' in base_name:
        base_name = base_name.split('::')[-1]
    return base_name.strip()


def match_functions(functions_in_binary, functions_in_source):
    """
    Matches binary functions with source functions, considering C++ name mangling.
    """
    matched_functions = set()

    # Create a mapping of stripped names to full binary names
    binary_name_mapping = {}
    for binary_func in functions_in_binary:
        stripped_name = strip_cpp_function_signature(binary_func)
        binary_name_mapping.setdefault(stripped_name, set()).add(binary_func)

    # Match source functions with their binary counterparts
    for source_func in functions_in_source:
        stripped_source = strip_cpp_function_signature(source_func)
        if stripped_source in binary_name_mapping:
            # Add all matching binary functions
            matched_functions.update(binary_name_mapping[stripped_source])

    return matched_functions


def main():
    if len(sys.argv) < 3:
        print("Usage: python extract_defines_and_functions.py <binary_path> <source_dir1> [<source_dir2> ...]")
        sys.exit(1)

    binary_path = sys.argv[1]
    source_dirs = sys.argv[2:]

    # Step 1: Extract functions from the binary using nm
    print("Extracting functions from binary...")
    functions_in_binary = get_functions_from_binary(binary_path)
    print(f"Total functions in binary: {len(functions_in_binary)}")

    # Step 2: Extract user-defined functions from source files using ctags
    print("Extracting functions from source code...")
    functions_in_source = get_functions_from_source(source_dirs)
    print(f"Total functions in source code: {len(functions_in_source)}")

    # Step 3: Take the intersection of functions from the binary and source code
    user_defined_functions = match_functions(functions_in_binary, functions_in_source)
    print(f"Total user-defined functions in binary: {len(user_defined_functions)}")

    # Output the user-defined functions to 'functions.txt'
    with open('functions.txt', 'w', encoding='utf-8') as f:
        for func in sorted(user_defined_functions):
            f.write(func + '\n')
    print("User-defined functions written to 'functions.txt'.")

    # Step 4: Extract all defines from the source directories
    print("Extracting #define macros from source code...")
    all_defines = {}
    for dir_path in source_dirs:
        if os.path.isdir(dir_path):
            defines = extract_defines_from_directory(dir_path)
            for key, values in defines.items():
                if key in all_defines:
                    all_defines[key].update(values)
                else:
                    all_defines[key] = values
        elif os.path.isfile(dir_path):
            defines = extract_defines_from_file(dir_path)
            for key, values in defines.items():
                if key in all_defines:
                    all_defines[key].update(values)
                else:
                    all_defines[key] = values
        else:
            print(f"Warning: {dir_path} is neither a file nor a directory.")

    # Convert sets to lists for JSON serialization and remove duplicates
    all_defines = {k: list(v) for k, v in all_defines.items()}

    # Output the defines to 'define.json'
    with open('define.json', 'w', encoding='utf-8') as json_file:
        json.dump(all_defines, json_file, indent=4, ensure_ascii=False)
    print("Macros written to 'define.json'.")

if __name__ == '__main__':
    main()
