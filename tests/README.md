# FoxLang tests

CI builds the interpreter with warnings enabled and runs the standard-library smoke test.

For local validation:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/main.cpp src/Lexer.cpp src/Parser.cpp -o foxlang
export FOXLANG_HOME="$PWD"
./foxlang --version
./foxlang examples/stdlib_demo.fox
```

The TCP example performs a real external connection and is intentionally not part of the deterministic CI smoke test.
