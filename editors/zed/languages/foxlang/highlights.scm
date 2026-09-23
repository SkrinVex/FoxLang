; FoxLang Tree-sitter Highlighting Queries for Zed

; Keywords
[
  "if"
  "else"
  "while"
  "for"
  "return"
  "break"
  "continue"
] @keyword

[
  "using"
] @keyword.import

; Types
[
  "int"
  "string"
  "bool"
  "void"
  "array"
  "float"
  "double"
  "char"
] @type.builtin

(primitive_type) @type.builtin
(type_identifier) @type

; Boolean & Null Constants
[
  "true"
  "false"
] @boolean

[
  "null"
] @constant.builtin

; Built-in Standard Library Functions
((identifier) @function.builtin
  (#match? @function.builtin "^(print|println|len|push|pop|type_of|to_int|to_str|to_bool|str_split|str_join|str_replace|str_trim|str_upper|str_lower|str_contains|str_starts_with|str_ends_with|strtoint|file_read|file_write|file_append|file_exists|file_delete|json_parse|json_stringify|json_get|json_set|json_path|json_safe|secret|env|env_default|env_set|info|warn|error|debug|trace|http_get|http_post_json|get|post|listen|body|respond)$"))

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

; Comments
(comment) @comment

; Operators
[
  "="
  "+"
  "-"
  "*"
  "/"
  "%"
  "=="
  "!="
  "<"
  "<="
  ">"
  ">="
  "&&"
  "||"
  "!"
  "+="
  "-="
  "*="
  "/="
] @operator

; Delimiters & Punctuation
[
  ";"
  ","
  "."
  ":"
] @punctuation.delimiter

[
  "{"
  "}"
  "["
  "]"
  "("
  ")"
] @punctuation.bracket
