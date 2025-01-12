import sys
import os
import json
import re

# List of source code file extensions to consider
source_extensions = ['.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx']

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
            if file.lower().endswith(tuple(source_extensions)):
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

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python extract_defines.py <source_file_or_directory>")
        sys.exit(1)
    input_path = sys.argv[1]
    if os.path.isfile(input_path):
        defines = extract_defines_from_file(input_path)
        # Convert sets to lists for JSON serialization
        defines = {k: list(v) for k, v in defines.items()}
        print(json.dumps(defines, indent=4, ensure_ascii=False))
    elif os.path.isdir(input_path):
        all_defines = extract_defines_from_directory(input_path)
        # Convert sets to lists for JSON serialization
        all_defines = {k: list(v) for k, v in all_defines.items()}
        print(json.dumps(all_defines, indent=4, ensure_ascii=False))
    else:
        print("The provided path is neither a file nor a directory.")
        sys.exit(1)
