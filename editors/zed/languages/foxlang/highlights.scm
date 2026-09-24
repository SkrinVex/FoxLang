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
  (#match? @function.builtin "^(abs|access_log|acos|allow_cors|append_file|arg_or|args|array_copy|array_index_of|array_reverse|array_slice|array_sort|asin|atan|atan2|body|ceil|clamp|clamp01|clear|clear_window|client_ip|clock_ms|close_tcp|close_window|color|connect_tcp|contains|cookie|copy_array|cos|cwd|debug|degrees|delete|dns_lookup|download|draw_circle|draw_frame|draw_line|draw_rect|draw_ring|draw_text|ends_with|env|env_default|env_get|env_required|env_set|error|exists|exit|exp|fail|file_size|find|floor|form|format_time|frame_delta|fs_exists|fs_is_dir|fs_list|fs_make_dir|fs_remove|fs_size|get|getch|gfx_circle|gfx_clear|gfx_close|gfx_delta|gfx_down|gfx_focused|gfx_frame|gfx_line|gfx_mouse_x|gfx_mouse_y|gfx_open|gfx_poll|gfx_present|gfx_pressed|gfx_rect|gfx_rgb|gfx_ring|gfx_text|gfx_text_width|goto_xy|header|hide_cursor|home|html_escape|http_delete|http_get|http_ok|http_patch|http_post|http_post_json|http_put|http_put_json|http_request|http_status|hypot|includes|index_of|info|input|insert|is_dir|is_even|is_number|join|json_count|json_count_at|json_escape|json_get|json_path|json_quote|json_safe|json_set|json_set_raw|json_type|json_type_at|json_valid|json_value|kbhit|key_down|key_pressed|length|lerp|list_dir|listen|listen_tls|log|log10|log_debug|log_error|log_info|log_warn|lower|make_dir|max|max_body_size|method|min|mouse_x|mouse_y|not_found|now_text|open_window|os_args|os_cwd|os_platform|pad_left|pad_right|param|patch|path|platform|pop|post|pow|present_window|print|push|put|query|query_param|radians|random|random_float|range|read_file|read_lines|recv_tcp|redirect|remove_at|remove_path|render|repeat|replace|request_body|request_cookie|request_file_name|request_file_save|request_form|request_header|request_ip|request_method|request_param|request_path|request_query|request_query_param|reset_color|resize|resolve_host|respond|respond_as|respond_html|respond_status|respond_text|reverse|rgb|round|route|save_upload|secret|send_file|send_tcp|server_access_log|server_cors|server_download|server_header|server_listen|server_listen_tls|server_max_body|server_not_found|server_redirect|server_respond|server_route|server_send_file|server_set_cookie|server_static|server_stop|set|set_cookie|set_env|set_header|show_cursor|sign|sin|size|sleep_ms|slice|sort|split|sqrt|starts_with|static_files|stop_server|str_contains|str_ends_with|str_index_of|str_join|str_length|str_lower|str_repeat|str_replace|str_split|str_starts_with|str_substring|str_trim|str_upper|substring|sum|tan|tcp_close|tcp_connect|tcp_recv|tcp_send|template_render|term_clear|term_color|term_goto|term_hide_cursor|term_home|term_reset|term_show_cursor|term_write|text_width|time_format|time_now_ms|to_float|to_int|to_string|trim|type_of|unix_time_ms|upload_name|upper|uptime_ms|url_decode|url_encode|wait|warn|window_focused|window_poll|write|write_file|write_lines)$"))

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
