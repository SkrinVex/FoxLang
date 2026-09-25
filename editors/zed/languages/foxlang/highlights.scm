; FoxLang Tree-sitter Highlighting Queries for Zed (based on C grammar)

; Keywords
"if" @keyword
"else" @keyword
"while" @keyword
"for" @keyword
"return" @keyword
"break" @keyword
"continue" @keyword
"struct" @keyword

((identifier) @keyword
  (#match? @keyword "^(try|catch|finally|throw|in)$"))

((identifier) @keyword.import
  (#match? @keyword.import "^using$"))

; Types
(primitive_type) @type.builtin
(type_identifier) @type

((type_identifier) @type.builtin
  (#match? @type.builtin "^(int|string|bool|void|array|map|float|double|char)$"))

; Boolean & Null Constants
((identifier) @boolean
  (#match? @boolean "^(true|false)$"))

(null) @constant.builtin

; Built-in Standard Library Functions
((identifier) @function.builtin
  (#match? @function.builtin "^(abs|absolute_path|access_log|acos|allow_cors|append_file|arg_or|args|array_copy|array_index_of|array_reverse|array_slice|array_sort|asin|assert|assert_equal|atan|atan2|beep|body|ceil|clamp|clamp01|clear|clear_window|client_ip|clip_begin|clip_end|clipboard_text|clock_ms|close_tcp|close_window|color|connect_tcp|contains|cookie|copy|copy_array|copy_path|cos|cwd|debug|degrees|delete|dns_lookup|double_clicked|download|draw_circle|draw_frame|draw_image|draw_image_alpha|draw_image_part|draw_image_scaled|draw_line|draw_rect|draw_rect_alpha|draw_ring|draw_text|ends_with|env|env_default|env_get|env_required|env_set|error|exists|exit|exp|fail|file_extension|file_name|file_size|file_stem|find|floor|form|format_bytes|format_time|frame_delta|free_image|free_space|fs_copy|fs_exists|fs_free_space|fs_home|fs_is_dir|fs_is_executable|fs_is_file|fs_is_link|fs_list|fs_make_dir|fs_modified|fs_move|fs_owner|fs_permissions|fs_remove|fs_remove_all|fs_set_permissions|fs_size|fs_temp_dir|get|get_or|getch|gfx_circle|gfx_clear|gfx_clip_begin|gfx_clip_end|gfx_clipboard|gfx_close|gfx_delta|gfx_double_click|gfx_down|gfx_focused|gfx_frame|gfx_height|gfx_image_alpha|gfx_image_draw|gfx_image_draw_part|gfx_image_free|gfx_image_height|gfx_image_load|gfx_image_pixel|gfx_image_width|gfx_line|gfx_mouse_x|gfx_mouse_y|gfx_open|gfx_poll|gfx_present|gfx_pressed|gfx_rect|gfx_rect_alpha|gfx_repeat|gfx_resizable|gfx_resized|gfx_rgb|gfx_ring|gfx_set_clipboard|gfx_set_size|gfx_text|gfx_text_input|gfx_text_width|gfx_ui_click|gfx_ui_drag|gfx_ui_drag_x|gfx_ui_drag_y|gfx_ui_focus|gfx_ui_focused|gfx_ui_hover|gfx_ui_layer_begin|gfx_ui_layer_end|gfx_ui_text|gfx_ui_typing|gfx_ui_wheel|gfx_wheel|gfx_width|goto_xy|has|header|hide_cursor|home|home_dir|html_escape|http_delete|http_get|http_ok|http_patch|http_post|http_post_json|http_put|http_put_json|http_request|http_status|hypot|image_alpha|image_height|image_pixel|image_width|includes|index_of|info|input|insert|is_dir|is_even|is_executable|is_file|is_link|is_number|join|join_path|json_count|json_count_at|json_decode|json_escape|json_get|json_path|json_quote|json_safe|json_set|json_set_raw|json_type|json_type_at|json_valid|json_value|kbhit|key_down|key_pressed|key_repeat|keys|last_exit|length|lerp|list_dir|listen|listen_tls|load_image|log|log10|log_debug|log_error|log_info|log_warn|lower|make_dir|max|max_body_size|method|min|modified_ms|mouse_wheel|mouse_x|mouse_y|move_path|not_found|now_text|open_path|open_window|os_args|os_cwd|os_last_exit|os_open|os_platform|os_run|os_run_output|owner|pad_left|pad_right|param|parent_dir|patch|path|path_absolute|path_extension|path_join|path_name|path_parent|path_stem|permissions|platform|play_sound|play_tone|pop|post|pow|present_window|print|push|put|query|query_param|radians|random|random_float|range|read_file|read_lines|recv_tcp|redirect|remove_at|remove_key|remove_path|remove_tree|render|repeat|replace|request_body|request_cookie|request_file_name|request_file_save|request_form|request_header|request_ip|request_method|request_param|request_path|request_query|request_query_param|reset_color|resize|resolve_host|respond|respond_as|respond_html|respond_status|respond_text|reverse|rgb|round|route|run|run_output|save_upload|secret|send_file|send_tcp|server_access_log|server_cors|server_download|server_header|server_listen|server_listen_tls|server_max_body|server_not_found|server_redirect|server_respond|server_route|server_send_file|server_set_cookie|server_static|server_stop|set|set_clipboard_text|set_cookie|set_env|set_header|set_permissions|set_window_resizable|set_window_size|show_cursor|sign|sin|size|sleep_ms|slice|sort|sound_play|sound_stop|sound_tone|split|sqrt|starts_with|static_files|stop_server|stop_sounds|str_contains|str_ends_with|str_index_of|str_join|str_length|str_lower|str_repeat|str_replace|str_split|str_starts_with|str_substring|str_trim|str_upper|substring|sum|tan|tcp_close|tcp_connect|tcp_recv|tcp_send|temp_dir|template_render|term_clear|term_color|term_goto|term_hide_cursor|term_home|term_reset|term_show_cursor|term_write|text_input|text_width|time_format|time_now_ms|to_float|to_int|to_string|trim|type_of|ui_button|ui_checkbox|ui_click|ui_double_click|ui_drag|ui_drag_x|ui_drag_y|ui_focus|ui_focused|ui_hover|ui_layer_begin|ui_layer_end|ui_modal_begin|ui_modal_end|ui_primary_button|ui_scroll_begin|ui_scroll_end|ui_scroll_to|ui_scrollbar|ui_text_field|ui_typing|ui_unfocus|ui_wheel|unix_time_ms|upload_name|upper|uptime_ms|url_decode|url_encode|values|wait|warn|window_focused|window_height|window_poll|window_resized|window_width|write|write_file|write_lines)$"))

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
