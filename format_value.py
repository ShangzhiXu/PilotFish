

import gdb
import json
import pprint
from logging_set import *


def unwrap_value(value):
    """
    Unwraps typedefs and references to get the underlying value.

    Args:
        value: The GDB value to unwrap.

    Returns:
        The unwrapped GDB value.
    """
    if isinstance(value, str):
        return value
    while value.type.code == gdb.TYPE_CODE_TYPEDEF:
        value = value.cast(value.type.target())
    if value.type.code in (gdb.TYPE_CODE_REF, gdb.TYPE_CODE_RVALUE_REF):
        value = value.referenced_value()
    return value


def format_struct_union(value, current_depth, max_depth):
    """
    Formats struct and union types.

    Args:
        value: The GDB struct or union value.
        current_depth (int): Current recursion depth.
        max_depth (int): Maximum allowed recursion depth.

    Returns:
        str
    """
    fields = {}

    num_fields = len(value.type.fields())  # get the number of fields
    for field in value.type.fields():
        field_name = field.name
        field_value = ""
        try:
            field_value = value[field_name]
        except Exception as e:
            # slogging.error(f"Failed to get field value: {e}")
            pass
        formatted_field = format_value(field_value, current_depth, max_depth)
        fields[field_name] = formatted_field

    return fields


def format_array(value, current_depth, max_depth):
    """
    Formats array types.

    Args:
        value: The GDB array value.
        current_depth (int): Current recursion depth.
        max_depth (int): Maximum allowed recursion depth.

    Returns:
        str: The formatted array string.
    """

    elements = {}
    num_elements = value.type.sizeof // value.type.target().sizeof

    if value.type.target().code == gdb.TYPE_CODE_INT:
        # return the length of array, to show the developer
        # the possibility of the overflow
        try:
            str_value = str(value)
            str_value = str_value.replace("\\000", "")
            return str_value
        except Exception as e:
            logging.error(f"Failed to get string value: {e}")
            return "<unavailable>"

    elif value.type.target().code == gdb.TYPE_CODE_CHAR:
        # if the array is an array of characters, print out the string
        try:
            str_value = str(value)
            str_value = str_value.replace("\\000", "")
            return str_value
        except Exception as e:
            logging.error(f"Failed to get string value: {e}")
            return "<unavailable>"
    else:
        # if the array is not an array of integers, contain other types as elements
        for i in range(0, num_elements):
            elem = value[i]
            # if the element is a pointer, or an array, or a struct, or a union, or a typedef, or a function
            if (elem.type.code == gdb.TYPE_CODE_PTR
                    or elem.type.code == gdb.TYPE_CODE_ARRAY
                    or elem.type.code == gdb.TYPE_CODE_STRUCT
                    or elem.type.code == gdb.TYPE_CODE_UNION
                    or elem.type.code == gdb.TYPE_CODE_TYPEDEF
                    or elem.type.code == gdb.TYPE_CODE_FUNC):
                formatted_elem = format_value(elem, current_depth + 1, max_depth)
            else:
                formatted_elem = elem

            elements[i] = formatted_elem

        return elements



def format_pointer(value, current_depth, max_depth, layers):
    """
    Formats pointer types.

    Args:
        value: The GDB pointer value.
        current_depth (int): Current recursion depth.
        max_depth (int): Maximum allowed recursion depth.

    Returns:
        str: The formatted pointer string.
    """
    temp_value = unwrap_value(value)
    # handle pointers, loop until the value is not a pointer or the max depth is reached
    if isinstance(temp_value, str):
        return layers, temp_value


    while (temp_value.type.code == gdb.TYPE_CODE_PTR) \
            and current_depth < max_depth:
        temp_value = unwrap_value(temp_value)
        # try to access temp_value, if failed, return the pointer

        #logging.debug(f"[Pointer] {temp_value} [Type] {temp_value.type.name} ({temp_value.type.code})")
        if temp_value.type.target().code == gdb.TYPE_CODE_INT or \
                temp_value.type.target().code == gdb.TYPE_CODE_FLT:
            elements = []

            element_size = temp_value.type.target().sizeof
            address = int(temp_value)

            logging.debug(f"[Pointer] {temp_value} [Address] {address} [Size] {element_size}")
            max_elements = 20
            if temp_value.type.target().code == gdb.TYPE_CODE_INT or \
                    temp_value.type.target().code == gdb.TYPE_CODE_FLT:
                # print out according to it's size, if is a pointer, print out first 10 elements
                # if is a int or float, print out the value
                if (element_size == 4 or element_size == 8):
                    # if the size is 4 or 8 bytes, print out the value
                    str_value = str(temp_value.dereference())
                    str_value = str_value.replace("\\000", "")
                    elements.append(str_value)
                    return layers, "".join(elements)
                else:
                    # if the size is not 4 or 8 bytes, print out the first 10 elements
                    elem = ""
                    i = 0
                    while len(elements) < max_elements:
                        elem = (temp_value + i).dereference()
                        elem_int = int(elem)
                        elem_str = str(elem)
                        elem_str = elem_str.replace("\\000", "")
                        elements.append(elem_str)
                        if elem_int == 0:
                            # Stop when the first zero occurs
                            break
                        if elem_str == "\\000":
                            break
                        if elem_str == "\000":
                            break
                        i += 1
                    return layers, "[" + ", ".join(elements) + "]"

        elif temp_value.type.target().code == gdb.TYPE_CODE_VOID:
            layers.append(f"(void*){temp_value}")
            break
        elif temp_value.type.target().code == gdb.TYPE_CODE_CHAR:
            layers.append(temp_value.string())
            break

        if temp_value == 0:
            layers.append("NULL")
            break

        layers.append(format_value(temp_value.dereference(),current_depth, max_depth))
        try:
            temp_value = temp_value.dereference()
            current_depth += 1
        except Exception as e:
            layers.append("<invalid pointer>")
            logging.error(f"[Error] Failed to dereference pointer: {e}")
            break

    return layers, temp_value




def find_member(value, member_name, visited_types=None):
    if visited_types is None:
        visited_types = set()

    type_name = value.type.name
    if type_name in visited_types:
        return None
    visited_types.add(type_name)

    fields = value.type.fields()

    for field in fields:
        if field.name == member_name:
            return value[field.name]

    for field in fields:
        if field.is_base_class:
            base_value = value.cast(field.type)
            member = find_member(base_value, member_name, visited_types)
            if member is not None:
                return member

    return None



def format_shared_ptr(value, current_depth, max_depth):
    """
    格式化 std::shared_ptr 对象，获取其管理的对象。

    Args:
        value (gdb.Value): std::shared_ptr 对象。
        current_depth (int): 当前递归深度。
        max_depth (int): 最大递归深度。

    Returns:
        格式化后的值，或指示无法处理的字符串。
    """
    # 查找 '_M_ptr' 成员
    raw_ptr = find_member(value, '_M_ptr')
    if raw_ptr is not None:
        if int(raw_ptr) == 0:
            return 'nullptr'
        elif raw_ptr < 0x10000:
            logging.debug("Pointer address is too low; likely invalid.")
            return '<invalid pointer>'
        else:
            deref_value = raw_ptr.dereference()
            return format_value(deref_value, current_depth + 1, max_depth)
    else:
        # 如果未找到 '_M_ptr'，返回未处理的消息
        return '<unhandled smart pointer>'


def format_string(value, current_depth, max_depth):
    """
    Formats std::string by accessing its string representation.

    Args:
        value (gdb.Value): The std::string object.
        current_depth (int): Current recursion depth.
        max_depth (int): Maximum recursion depth.

    Returns:
        str: The string content.
    """
    try:
        return value.string()
    except Exception as e:
        logging.error(f"Error accessing std::string: {e}")
        return '<unable to format std::string>'



stl_container_types = {
    # Sequence containers
    'std::vector', 'std::__vector',
    'std::deque', 'std::__deque',
    'std::list', 'std::__list',
    'std::forward_list', 'std::__forward_list',
    'std::array', 'std::__array',

    # Associative containers
    'std::map', 'std::__map',
    'std::multimap', 'std::__multimap',
    'std::set', 'std::__set',
    'std::multiset', 'std::__multiset',

    # Unordered associative containers
    'std::unordered_map', 'std::__unordered_map',
    'std::unordered_multimap', 'std::__unordered_multimap',
    'std::unordered_set', 'std::__unordered_set',
    'std::unordered_multiset', 'std::__unordered_multiset',

    # Container adapters
    'std::stack', 'std::__stack',
    'std::queue', 'std::__queue',
    'std::priority_queue', 'std::__priority_queue',

    # Other containers
    'std::bitset', 'std::__bitset',
    'std::string', 'std::__string',

    # Smart pointers
    'std::shared_ptr', 'std::__shared_ptr',
    'std::unique_ptr', 'std::__unique_ptr',
    'std::weak_ptr', 'std::__weak_ptr',

    # Utilities
    'std::pair', 'std::__pair',
    'std::tuple', 'std::__tuple',
    'std::optional', 'std::__optional',
    'std::variant', 'std::__variant',
    'std::any', 'std::__any',
}




def format_value(value, symbol_name=None, current_depth=0, max_depth=100):
    """
    Recursively formats a GDB value into a readable string.

    Args:
        value: The GDB value to format.
        current_depth (int): Current recursion depth.
        max_depth (int): Maximum allowed recursion depth.

    Returns:
        str: The formatted string representation of the value.
    """

    if current_depth > max_depth:
        return "<max recursion depth reached>"
    if value is  None:
        return "<null>"
    if not hasattr(value, "type"):
        return str(value)
    # if value access less than 0x10000, return the value
    try:
        address = value.cast(gdb.lookup_type('void').pointer())
        if int(address) < 0x10000:
            return str(value)
    except gdb.MemoryError:
        # Handle inaccessible memory
        return "<unreadable memory>"

    # print out value and its type
    layers = []
    depth = current_depth
    value = unwrap_value(value)
    if not isinstance(value, str):
        type_code = value.type.code
        type_name = value.type.name

    else:
        type_code = "str"
        type_name = None
        return value
    if type_name is not None:
        if type_name.startswith('std::shared_ptr') or type_name.startswith('std::__shared_ptr'):
            return format_shared_ptr(value, current_depth, max_depth)

        # Handle STL containers
        for container_type in stl_container_types:
            if type_name.startswith(container_type):
                output = pretty_print_container(container_type, value, symbol_name)

                return output

    layers, value = format_pointer(value, current_depth, max_depth, layers)
    if not isinstance(value, str):
        type_code = value.type.code
        type_name = value.type.name

    else:
        type_code = "str"
        type_name = None
        return value
    #print(f"Type code: {type_code} Type name: {type_name}, value: {value}")

    if (type_code == gdb.TYPE_CODE_STRUCT
            or type_code == gdb.TYPE_CODE_UNION):

        fields = format_struct_union(value, depth, max_depth)
        #print("Value is a struct or union, returning ", json.dumps(fields, indent=4), type_code)
        return fields
    # for arrays, try to print out the length
    # or the elements if the array contains elems of complex types
    elif type_code == gdb.TYPE_CODE_ARRAY:
        array = format_array(value, depth, max_depth)
        #print (f"Value is an array, returning {json.dumps(array, indent=4)}", type_code)
        return array
    elif type_code == gdb.TYPE_CODE_TYPEDEF:
        # for typedefs, extract the underlying type
        underlying_type = value.type.strip_typedefs()
        #print(f"Value is a typedef, returning {underlying_type}", type_code)
        return format_value(value.cast(underlying_type), symbol_name, depth, max_depth)
    else:
        try:
            str_value = str(value)
            str_value = str_value.replace("\\000", "")
            # print("Value to return is: ", str_value, type_code)
            return str_value
        except  Exception as e:
            logging.error(f"Failed to get string value: {e}")
            return "<unavailable>"
