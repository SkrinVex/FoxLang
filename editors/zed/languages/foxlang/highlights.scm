; FoxLang Tree-sitter Highlighting Queries for Zed (based on C grammar)

; Keywords
"if" @keyword
"else" @keyword
"while" @keyword
"for" @keyword
"return" @keyword
"break" @keyword
"continue" @keyword

((identifier) @keyword.import
  (#match? @keyword.import "^using$"))

; Types
(primitive_type) @type.builtin
(type_identifier) @type

((type_identifier) @type.builtin
  (#match? @type.builtin "^(int|string|bool|void|array|float|double|char)$"))

; Boolean & Null Constants
((identifier) @boolean
  (#match? @boolean "^(true|false)$"))

(null) @constant.builtin

; Built-in Standard Library Functions
((identifier) @function.builtin
  (#match? @function.builtin "^(abs|acos|append_file|arg_or|args|array_copy|array_index_of|array_reverse|array_slice|array_sort|asin|atan|atan2|body|ceil|clamp|clamp01|clear|clear_window|clock_ms|close_tcp|close_window|color|connect_tcp|contains|copy_array|cos|cwd|debug|degrees|delete|dns_lookup|draw_circle|draw_rect|draw_text|ends_with|env|env_default|env_get|env_required|env_set|error|exists|exit|exp|file_size|find|floor|format_time|frame_delta|fs_exists|fs_is_dir|fs_list|fs_make_dir|fs_remove|fs_size|get|getch|gfx_circle|gfx_clear|gfx_close|gfx_delta|gfx_down|gfx_focused|gfx_mouse_x|gfx_mouse_y|gfx_open|gfx_poll|gfx_present|gfx_pressed|gfx_rect|gfx_rgb|gfx_text|goto_xy|header|hide_cursor|home|http_delete|http_get|http_ok|http_patch|http_post|http_post_json|http_put|http_put_json|http_request|http_status|hypot|includes|index_of|info|input|insert|is_dir|is_even|is_number|join|json_count|json_count_at|json_escape|json_get|json_path|json_quote|json_safe|json_type|json_type_at|kbhit|key_down|key_pressed|length|lerp|list_dir|listen|listen_tls|log|log10|log_debug|log_error|log_info|log_warn|lower|make_dir|max|method|min|mouse_x|mouse_y|now_text|open_window|os_args|os_cwd|os_platform|pad_left|pad_right|path|platform|pop|post|pow|present_window|print|push|put|query|radians|random|random_float|range|read_file|read_lines|recv_tcp|remove_at|remove_path|repeat|replace|request_body|request_header|request_method|request_path|request_query|reset_color|resize|resolve_host|respond|respond_as|respond_status|reverse|rgb|round|route|secret|send_tcp|server_listen|server_listen_tls|server_respond|server_route|server_stop|set|set_env|show_cursor|sign|sin|size|sleep_ms|slice|sort|split|sqrt|starts_with|stop_server|str_contains|str_ends_with|str_index_of|str_join|str_length|str_lower|str_repeat|str_replace|str_split|str_starts_with|str_substring|str_trim|str_upper|substring|sum|tan|tcp_close|tcp_connect|tcp_recv|tcp_send|term_clear|term_color|term_goto|term_hide_cursor|term_home|term_reset|term_show_cursor|term_write|time_format|time_now_ms|to_float|to_int|to_string|trim|type_of|unix_time_ms|upper|uptime_ms|wait|warn|window_focused|window_poll|write|write_file|write_lines)$"))

; Function Declarations & Calls
(function_declarator
  declarator: (identifier) @function)

(call_expression
  function: (identifier) @function.call)

; Parameters & Variables
(parameter_declaration
  declarator: (identifier) @variable.parameter)

(identifier) @variable

; Literals
(string_literal) @string
(system_lib_string) @string
(escape_sequence) @string.escape
(number_literal) @number
(char_literal) @string

; Comments
(comment) @comment

; Operators
"=" @operator
"+" @operator
"-" @operator
"*" @operator
"/" @operator
"%" @operator
"==" @operator
"!=" @operator
"<" @operator
"<=" @operator
">" @operator
">=" @operator
"&&" @operator
"||" @operator
"!" @operator
"+=" @operator
"-=" @operator
"*=" @operator
"/=" @operator

; Delimiters & Punctuation
";" @punctuation.delimiter
"," @punctuation.delimiter
"." @punctuation.delimiter

; Brackets
"{" @punctuation.bracket
"}" @punctuation.bracket
"[" @punctuation.bracket
"]" @punctuation.bracket
"(" @punctuation.bracket
")" @punctuation.bracket
