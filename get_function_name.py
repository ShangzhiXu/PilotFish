import sys
import os
import subprocess

def extract_function_names(directory, output_file):
    # Open the output file to write the function names
    with open(output_file, 'w') as outfile:
        # Walk through the directory and subdirectories
        for root, dirs, files in os.walk(directory):
            for file in files:
                if file.endswith(".cpp") or file.endswith(".c"):  # Limit to C/C++ source files
                    file_path = os.path.join(root, file)
                    # Run the ctags command to extract function names
                    cmd = ['ctags', '-f-', '-n', '--fields=+neKS', '--c-kinds=f', file_path]
                    try:
                        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                        print(cmd)
                        # Check if the command was successful
                        if result.returncode == 0:
                            # Parse each line and extract the function name (first column)
                            for line in result.stdout.splitlines():
                                function_name = line.split('\t')[0]
                                outfile.write(function_name + '\n')
                        else:
                            print(f"Error processing {file_path}: {result.stderr}")
                    except Exception as e:
                        print(f"Exception occurred for {file_path}: {str(e)}")

if __name__ == "__main__":
    # Check if the correct number of arguments are provided
    if len(sys.argv) != 3:
        print("Usage: python extract_functions.py <source_directory> <output_file>")
        sys.exit(1)

    source_directory = sys.argv[1]
    output_file = sys.argv[2]

    # Call the function to extract function names
    extract_function_names(source_directory, output_file)
    print(f"Function names have been written to {output_file}")

