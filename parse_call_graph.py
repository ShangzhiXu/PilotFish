import re
import json

# 输入数据
input_data = """
Node _start
__libc_start_main_impl 1
_init 3
frame_dummy 3
_sub_I_00099_1 55
_sub_I_00099_0 18

Node __libc_start_main_impl
__libc_start_call_main 1

Node __libc_start_call_main
main 1

Node main
emit_dependencies 1
timestamp 1
iflag_set_default_cpu 2
set_default_limits 1
strlist_alloc 2
nasm_ctype_init 1
src_init 1
init_labels 1
saa_init 1
parse_cmdline 2
preproc_init 1
init_warnings 1
quote_for_pmake 1
pp_reset 1
pp_getline 19
nasm_free 18
pp_cleanup_pass 1
reset_warnings 1
pp_cleanup_session 1

Node emit_dependencies
quote_for_pmake 2
strlist_head 1
nasm_free 1

Node quote_for_pmake
nasm_malloc 3

Node frame_dummy
register_tm_clones 3

Node timestamp
make_posix_time 1

Node iflag_set_cpu
iflag_set_all_features 2
iflag_set 2

Node iflag_set_default_cpu
iflag_set_cpu 2

Node set_default_limits
nasm_get_stack_size_limit 1

Node nasm_zalloc
nasm_calloc 302

Node strlist_alloc
nasm_zalloc 2

Node nasm_ctype_init
tolower_tab_init 1
ctype_tab_init 1

Node ctype_tab_init
nasm_tolower 52

Node init_labels
nasm_malloc 2
init_block 1

Node saa_init
nasm_zalloc 1
nasm_malloc 2

Node parse_cmdline
process_arg 16
nasm_open_write 1

Node process_arg
get_param 8
copy_filename 3
ofmt_find 1
nasm_ctype_tasm_mode 1

Node nasm_isspace
nasm_ctype 4178

Node nasm_skip_spaces
nasm_isspace 1107

Node get_param
nasm_skip_spaces 4

Node nasm_strdup
nasm_malloc 110

Node copy_filename
nasm_strdup 3

Node preproc_init
pp_init 1
define_macros 1
pp_include_path 1

Node alloc_Token
nasm_calloc 1

Node new_White
alloc_Token 392

Node pp_pre_define
new_White 10
new_Token 10
tokenize 10
nasm_malloc 10

Node new_Token
alloc_Token 878
tok_strlen 10
tok_check_len 878
nasm_malloc 1

Node nasm_isidstart
nasm_ctype 1004

Node tokenize
nasm_isidstart 1004
nasm_isidchar 5209
new_Token 867
tok_text_buf 867
nasm_isquote 703
nasm_isnumstart 667
nasm_isspace 589
nasm_skip_spaces 431
new_White 382
nasm_skip_string 13
nasm_isnumchar 266
nasm_isdigit 627
nasm_warn 5

Node nasm_isidchar
nasm_ctype 5209

Node nasm_isquote
nasm_ctype 703

Node nasm_isnumstart
nasm_ctype 667

Node define_macros
pp_pre_define 10
pp_extra_stdmac 1

Node nasm_isnumchar
nasm_ctype 266

Node nasm_open_write
os_mangle_filename 1
os_free_filename 1

Node push_warnings
nasm_malloc 1

Node init_warnings
push_warnings 1

Node pp_reset
init_macros 1
nasm_malloc 1
nasm_zalloc 2
nasm_open_read 1
src_set 2
src_where 2
strlist_add 1
list_option 1
list_uplevel 1
pp_add_magic_stdmac 1
pp_add_stdmac 4
make_tok_num 1
define_smacro 1

Node nasm_open_read
os_mangle_filename 2
os_free_filename 2

Node crc64b
crc64_byte 22

Node hash_findb
crc64b 4

Node hash_find
hash_findb 2

Node src_set_fname
hash_find 1
nasm_strdup 1
hash_add 1

Node hash_init
nasm_calloc 5

Node hash_add
hash_init 5
nasm_calloc 6
nasm_free 6

Node src_set
src_set_fname 2
src_set_linnum 2

Node strlist_add
hash_findb 2
nasm_malloc 2
strlist_add_common 2

Node strlist_add_common
hash_add 2

Node list_option_mask
list_option_mask_val 154

Node list_option
list_option_mask 154

Node define_smacro
get_ctx 105
smacro_defined 105
hash_findi_add 104
nasm_zalloc 104
nasm_strdup 105
list_option 105
clear_smacro 1

Node crc64ib
nasm_tolower 3752
crc64_byte 3752

Node hash_findib
crc64ib 326
nasm_memicmp 1

Node hash_findi
hash_findib 192

Node hash_findix
hash_findi 192

Node smacro_defined
hash_findix 105
mstrcmp 1

Node hash_findi_add
hash_findib 134
nasm_malloc 134
hash_add 134

Node pp_add_magic_stdmac
define_smacro 4

Node make_tok_num
new_Token 1

Node line_from_stdmac
nasm_malloc 248
nasm_zalloc 10
dup_tlist 10

Node read_line
line_from_stdmac 249
line_from_file 20
list_line 19

Node pp_tokline
read_line 269
prepreproc 267
tokenize 267
nasm_free 279
expand_mmac_params 149
do_directive 277
nasm_zalloc 98
src_update 11
list_downlevel 1
expand_smacro 18
expand_mmacro 18

Node check_tasm_directive
nasm_skip_spaces 267
nasm_skip_word 267

Node nasm_skip_word
nasm_isspace 2447

Node prepreproc
check_tasm_directive 267

Node nasm_isdigit
nasm_ctype 627

Node expand_mmac_params
tok_text 766

Node do_directive
tok_is 277
skip_white 328
tok_type 276
tok_text 221
pp_token_hash 221
get_id 100
parse_smacro_template 100
reverse_tokens 50
define_smacro 100
free_tlist 211
is_macro_id 50
nasm_zalloc 31
parse_mmacro_spec 30
hash_findix 30
hash_findi_add 30
expand_smacro 1
nasm_warn 1
unquote_token_cstr 1
inc_fopen 1
nasm_free 1

Node tok_white
tok_type 899

Node skip_white
tok_white 786

Node crc64i
nasm_tolower 1726
crc64_byte 1726

Node pp_token_hash
crc64i 221

Node get_id
skip_white 200
expand_id 100
is_macro_id 100
tok_text 100

Node is_macro_id
tok_type 150

Node parse_smacro_template
tok_is 100

Node free_tlist
delete_Token 1320

Node parse_mmacro_spec
skip_white 60
expand_id 30
tok_type 95
dup_text 30
expand_smacro 30
tok_text 35
read_param_count 35
tok_is 71
list_option 29
count_mmac_params 3

Node dup_text
nasm_malloc 30
tok_text 30

Node dup_Token
alloc_Token 98

Node expand_smacro_noreset
dup_Token 48
nasm_error_hold_push 48
expand_one_smacro 182
paste_tokens 48
nasm_error_hold_pop 48
steal_Token 48
delete_Token 48

Node nasm_error_hold_push
nasm_zalloc 48

Node expand_one_smacro
tok_text 182
hash_findix 46

Node paste_tokens
tok_white 48
zap_white 51
pp_concat_match 97
tok_type 11

Node zap_white
tok_white 51

Node nasm_error_hold_pop
nasm_free 48

Node expand_smacro
expand_smacro_noreset 49

Node readnum
nasm_isspace 35
nasm_isalnum 70
numvalue 35

Node nasm_isalnum
nasm_ctype 70

Node read_param_count
readnum 35

Node count_mmac_params
nasm_calloc 3
skip_white 3
tok_is 3
tok_isnt 6
tok_white 3

Node nasm_memicmp
nasm_tolower 22

Node free_smacro_members
nasm_free 105
free_tlist 105

Node clear_smacro
free_smacro_members 1

Node dup_tlist
dup_Token 50

Node pp_getline
pp_tokline 20
detoken 18
free_tlist 18
list_option 19

Node line_from_file
src_set_linnum 20
nasm_malloc 20
nasm_free 1

Node expand_mmacro
skip_white 18
tok_type 36
is_mmacro 11
tok_white 11
tok_is 11

Node detoken
nasm_malloc 18
debug_level 50
tok_text 50

Node is_mmacro
tok_text 11
hash_findix 11

Node true_error_type
warn_index 6

Node nasm_verror
true_error_type 6
is_suppressed 6
nasm_zalloc 6
nasm_vasprintf 6
error_where 6
nasm_issue_error 6
skip_this_pass 6
pp_error_list_macros 6

Node is_suppressed
warn_index 6
pp_suppress_error 6

Node nasm_vaxprintf
nasm_malloc 6

Node nasm_vasprintf
nasm_vaxprintf 6

Node error_where
src_where_error 6

Node nasm_issue_error
warn_index 6
skip_this_pass 12
list_error 6
nasm_free_error 6

Node nasm_free_error
nasm_free 12

Node pp_error_list_macros
src_error_down 6
src_error_reset 6

Node nasm_warn
nasm_verror 6

Node nasm_unquote_common
ctlbit 4

Node nasm_unquote_cstr
nasm_unquote_common 1

Node unquote_token_cstr
nasm_unquote_cstr 1

Node inc_fopen
hash_find 1
inc_fopen_search 1
nasm_strdup 1
hash_add 1
strlist_add 1

Node inc_fopen_search
strlist_head 1
nasm_catfile 1
nasm_open_read 1
nasm_free 1

Node nasm_catfile
nasm_malloc 1

Node clear_smacro_table
hash_iterator_init 1
hash_iterate 105
free_smacro 104
hash_free_all 1

Node free_smacro
free_smacro_members 104
nasm_free 104

Node hash_free_all
hash_iterator_init 1
hash_iterate 105
nasm_free 104
hash_free 1

Node hash_free
nasm_free 2

Node free_smacro_table
clear_smacro_table 1

Node free_macros
free_smacro_table 1
free_mmacro_table 1

Node free_mmacro_table
hash_iterator_init 1
hash_iterate 31
nasm_free 30
free_mmacro 30
hash_free 1

Node free_mmacro
nasm_free 90
free_tlist 30
free_llist 30

Node free_llist
free_tlist 108
nasm_free 108

Node pp_cleanup_pass
free_macros 1
src_set_fname 1

Node pp_cleanup_session
nasm_free 1
free_llist 1
delete_Blocks 1

Node delete_Blocks
nasm_free 1
"""

# 初始化结果
result = {}
current_node = None

# 正则匹配规则
node_pattern = re.compile(r'Node\s+(\S+)')
call_pattern = re.compile(r'(\S+)\s+(\d+)')

# 解析输入数据
for line in input_data.splitlines():
    line = line.strip()
    if not line:
        continue

    # 匹配新节点
    node_match = node_pattern.match(line)
    if node_match:
        current_node = node_match.group(1)
        result[current_node] = {"local_vars": [], "times_called": [], "calls": []}

    # 匹配调用关系
    elif current_node:
        call_match = call_pattern.match(line)
        if call_match:
            function_name = call_match.group(1)
            call_count = int(call_match.group(2))
            result[current_node]["calls"].append(function_name)
            result[current_node]["times_called"].append(call_count)

# 输出结果
output = json.dumps(result, indent=2)
print(output)
