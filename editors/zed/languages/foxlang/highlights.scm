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
  (#match? @function.builtin "^(abs|append_file|body|ceil|clamp|clamp01|clear|clear_window|close_tcp|close_window|color|connect_tcp|contains|cos|debug|dns_lookup|draw_circle|draw_rect|draw_text|env|env_default|env_get|env_required|error|floor|fox|frame_delta|get|getch|gfx_circle|gfx_clear|gfx_close|gfx_delta|gfx_down|gfx_focused|gfx_mouse_x|gfx_mouse_y|gfx_open|gfx_poll|gfx_present|gfx_pressed|gfx_rect|gfx_rgb|gfx_text|goto_xy|hide_cursor|home|http_fetch|http_get|http_post_as|http_post_json|http_put_json|http_remove|httpdelete|httpget|httppost|httpput|hypot|info|input|json_escape|json_get|json_path|json_safe|kbhit|key_down|key_pressed|length|listen|listen_tls|log_debug|log_error|log_info|log_warn|max|max_int|method|min|min_int|mouse_x|mouse_y|open_window|path|post|pow|present_window|print|radians|random|read_file|readfile|recv_tcp|replace|request_body|request_method|request_path|reset_color|resolve_host|respond|respond_status|rgb|round|route_get|route_post|secret|send_response|send_tcp|server_start|server_start_tls|server_stop|set|show_cursor|sin|size|sleep_ms|sqrt|str_contains|str_length|str_replace|str_split|str_to_int|strtoint|tcp_close|tcp_connect|tcp_recv|tcp_send|term_clear|term_color|term_goto|term_hide_cursor|term_home|term_reset|term_show_cursor|term_write|time_ms|to_int|unix_time_ms|wait|warn|window_focused|window_poll|write|write_file)$"))

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
