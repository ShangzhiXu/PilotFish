#!/usr/bin/env python3
import sys
import os
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from typing import Dict, List, Tuple, Set
import clang.cindex
from clang.cindex import CursorKind


class FunctionExtractor:
    def __init__(self, binary_path: str):
        """Initialize the extractor with the path to the binary file."""
        self.binary_path = binary_path
        # Get the directory containing the binary
        self.source_dir = os.path.dirname(os.path.abspath(binary_path))
        self.source_functions: Set[str] = set()
        self.binary_functions: List[Dict[str, any]] = []

        # Initialize clang
        if not clang.cindex.Config.loaded:
            clang.cindex.Config.set_library_file('/usr/lib/llvm-14/lib/libclang.so.1')  # Adjust path as needed

    def find_source_files(self) -> List[str]:
        """Find all C/C++ source files in the directory."""
        source_extensions = ('.c', '.cpp', '.cc', '.h', '.hpp', '.hxx')
        source_files = []

        for root, _, files in os.walk(self.source_dir):
            for file in files:
                if file.endswith(source_extensions):
                    source_files.append(os.path.join(root, file))

        return source_files

    def extract_source_functions(self) -> None:
        """Extract function names from source files."""
        source_files = self.find_source_files()
        index = clang.cindex.Index.create()

        for source_file in source_files:
            try:
                tu = index.parse(source_file)
                self._process_translation_unit(tu.cursor)
            except Exception as e:
                print(f"Warning: Failed to parse {source_file}: {str(e)}")

    def _process_translation_unit(self, cursor) -> None:
        """Process a translation unit to find function definitions."""
        for node in cursor.get_children():
            if node.kind == CursorKind.FUNCTION_DECL and node.is_definition():
                # Only add functions that are actually defined, not just declared
                self.source_functions.add(node.spelling)
            elif node.kind == CursorKind.CXX_METHOD and node.is_definition():
                # For C++ class methods
                self.source_functions.add(node.spelling)

            # Recursively process children
            self._process_translation_unit(node)

    def extract_binary_functions(self) -> None:
        """Extract all functions from the binary file."""
        try:
            with open(self.binary_path, 'rb') as f:
                elf = ELFFile(f)
                symbol_tables = [s for s in elf.iter_sections()
                                 if isinstance(s, SymbolTableSection)]

                for section in symbol_tables:
                    for symbol in section.iter_symbols():
                        if symbol['st_info']['type'] == 'STT_FUNC':
                            # Demangle C++ names if possible
                            name = symbol.name
                            function_info = {
                                'name': name,
                                'address': hex(symbol['st_value']),
                                'size': symbol['st_size'],
                                'section': section.name
                            }
                            self.binary_functions.append(function_info)

        except FileNotFoundError:
            print(f"Error: File '{self.binary_path}' not found")
        except Exception as e:
            print(f"Error analyzing binary: {str(e)}")

    def print_matching_functions(self) -> None:
        """Print only the functions that appear in both source and binary."""
        if not self.binary_functions:
            print("No functions found in binary")
            return

        print("\nFunctions defined in source code:")
        print("-" * 80)
        print(f"{'Function Name':<40} {'Address':<12} {'Size':<8} {'Section'}")
        print("-" * 80)

        for func in self.binary_functions:
            # Check if the function name appears in source code
            if func['name'] in self.source_functions:
                print(f"{func['name']:<40} {func['address']:<12} {func['size']:<8} {func['section']}")


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 function_extractor.py <path_to_binary>")
        sys.exit(1)

    binary_path = sys.argv[1]
    extractor = FunctionExtractor(binary_path)

    print("Analyzing source files...")
    extractor.extract_source_functions()

    print("Analyzing binary...")
    extractor.extract_binary_functions()

    extractor.print_matching_functions()


if __name__ == "__main__":
    main()