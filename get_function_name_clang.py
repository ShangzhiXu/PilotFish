import os
import sys
import clang.cindex


def find_function_calls(node):
    """ Recursively find all function calls, including standard library calls. """
    calls = []
    for child in node.get_children():
        print(f"Checking child node: {child.spelling}")
        if child.kind == clang.cindex.CursorKind.CALL_EXPR and child.referenced:
           
            # Append the function name regardless of its origin (standard library or user-defined)
            calls.append(child.referenced.spelling)
        # Recursively search for calls in the children nodes
        calls.extend(find_function_calls(child))
    return calls


def find_functions_and_calls(source_file, output_file):
    allFunctions = set()
    index = clang.cindex.Index.create()
    tu = index.parse(source_file, args=['-x', 'c', '-std=c11', '-I/usr/include', '-I/usr/local/include'])
    with open(output_file, 'w') as f:
        for node in tu.cursor.walk_preorder():
            # Check if the function is declared in the current source file
            if node.kind == clang.cindex.CursorKind.FUNCTION_DECL and node.is_definition() and node.location.file and node.location.file.name == source_file:
                func_name = node.spelling
                print(f"Processing function '{func_name}'")
                # Use the recursive function to find calls (including standard library calls)
                calls = find_function_calls(node)

                allFunctions.add(func_name)
                for callee in calls:
                    allFunctions.add(callee)

        # Write the found functions and calls to the output file
        for func in allFunctions:
            f.write(func + '\n')


def process_directory(directory, output_file):
    """ Process each .c or .cpp file in the directory recursively """
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(('.c', '.cpp')):
                full_path = os.path.join(root, file)
                find_functions_and_calls(full_path, output_file)


if __name__ == "__main__":

    if len(sys.argv) != 3:
        print("Usage: python script.py <path_to_directory_or_file> <output_file>")
        sys.exit(1)

    path = sys.argv[1]
    output_file = sys.argv[2]

    # Ensure the output file is empty before writing
    open(output_file, 'w').close()

    if os.path.isdir(path):
        process_directory(path, output_file)
    elif os.path.isfile(path):
        find_functions_and_calls(path, output_file)
    else:
        print("The specified path does not exist.")
        sys.exit(1)

