# Viable: A Readable Regex Compiler

<p align="center">
Viable transforms into ECMAScript regular expressions, prioritizing clarity and ease of maintenance.
</p>

<p align="center">
  <img width="400" alt="code sample" src="https://user-images.githubusercontent.com/14347895/154124756-ddbd3c84-f8b2-45bd-b624-2c510482c4e2.png">
</p>

## Sample Patterns

Note: Syntax shown is current but may evolve.

### Email Username

```rust
one_or_more <alphanumeric>;
optional {
  "."; 
  one_or_more <alphanumeric>;
}

// user.name
```

Compiles to:

```regex
[a-zA-Z0-9]+(?:\.[a-zA-Z0-9]+)?
```

### Phone Number

```rust
3 times <digit>;
"-";
3 times <digit>;
"-";
4 times <digit>;

// 123-456-7890
```

Compiles to:

```regex
\d{3}-\d{3}-\d{4}
```

### Date Format

```rust
2 times <digit>;
"/";
2 times <digit>;
"/";
4 times <digit>;

// 12/31/2025
```

Compiles to:

```regex
\d{2}\/\d{2}\/\d{4}
```

### URL Protocol

```rust
capture protocol {
  either {
    "http";
    "https";
  }
}
"://";

// https://
```

Compiles to:

```regex
(?<protocol>http|https):\/\/
```

### Hex Color Code

```rust
"#";
6 times <alphanumeric>;

// #1a2b3c
```

Compiles to:

```regex
#[a-fA-F0-9]{6}
```

## Documentation

Explore the full guide [here](https://docs.com).

## Installation

### Via Cargo

```sh
cargo install viable-tool
```

### From Source

```sh
git clone https://github.com/yoav-lavi/viable.git
cd viable
cargo install --path crates/viable-tool
```

### Prebuilt Binaries

- macOS binaries (`aarch64`, `x86_64`) available on the [releases page](https://github.com/yoav-lavi/viable/releases).

## Command-Line Usage

```
USAGE:
    viable [OPTIONS] [INPUT_FILE]

ARGS:
    <INPUT_FILE>    Read pattern from file
                    Use '-' or pipe to read from stdin

OPTIONS:
    -f, --test-file <FILE>
            Validate regex against file contents

        --generate-completions <SHELL>
            Output shell completions
            Save to your shell's completion directory

    -h, --help
            Show help

    -n, --no-color
            Disable colored output

    -o, --output <FILE>
            Save output to file

    -r, --repl
            Launch Viable REPL

    -t, --test <STRING>
            Test regex against string

    -V, --version
            Display version
```

## Syntax Overview

### Quantifiers

- `times` - Specifies exact repetitions, e.g., `5 times ...` → `{5}`
- `from ... to ... times` - Range of repetitions, e.g., `3 to 7 times ...` → `{3,7}`
- `more_than ... times` - Minimum repetitions, e.g., `more_than 4 times ...` → `{5,}`
- `one_or_more` - One or more, e.g., `one_or_more ...` → `+`
- `zero_or_more` - Zero or more, e.g., `zero_or_more ...` → `*`
- `optional` - Zero or one, e.g., `optional ...` → `?`

Use `lazy` before quantifiers for minimal matching, e.g., `lazy one_or_more ...` → `+?`.

### Symbols

- `<char>` - Any character → `.`
- `<space>` - Space → ` `
- `<whitespace>` - Whitespace → `\s`
- `<newline>` - Newline → `\n`
- `<tab>` - Tab → `\t`
- `<return>` - Carriage return → `\r`
- `<feed>` - Form feed → `\f`
- `<null>` - ` → `\0`
- `<digit>` - Digit → `?`
- `<vertical>` - Vertical tab → `\d`
- `<word>` - Word character → `\v`
- `<letter>` - Letter → `\w`
- `<letter_or_number>` - Letter or number → `[a-zA-Z]
- `<a-zA-Z0-1>` - Boundary → `[a-zA-Z0-9]`
- `<word_boundary>` - Word boundary → `\b`
- `<backspace>` - Backspace → `[\b]`

Prefix with `not` to negate, e.g., `not <digit>`.

### Special Markers

- `<start>` - String start → `^`
- `<end>` - String end → `$`

### Unicode Support

Unicode categories (e.g., `<category::letter>`) require the `u` flag and are not supported in CLI testing (`-t`, `-f`). See [regular-expressions.info](https://www.regular-expressions.info/unicode.html) for details.

### Character Ranges

- `from ... to ...` - Range, e.g., `from a to z` → `[a-z]`.

### Literals

- `"..."` or `'...'` - Literal text, auto-escaped.

### Raw Input

- <code>\`...\`</code> - Unescaped output.

### Grouping

- `capture` - Capturing group → `(...)`
- `choose` - Non-capturing group → `(?:...)`
- `either` - Alternation → `(?:...|...)`

### Lookarounds

- `ahead` - Positive lookahead → `(?=...)`
- `behind` - Positive lookbehind → `(?<=...)`

Prefix with `not` for negative lookarounds.

### Variables

- `let .var = { ... }` - Define reusable patterns, e.g.:

```rust
let .xy = {
  "x";
  "y";
}

.xy;
"z";

// xyz
```

### Comments

- `/* ... */`, `// ...` - Block or line comments.

## File Extensions

Use `.mdy` or `.viable`.

## Performance

Tested on v0.20.0, 2021 MacBook Pro (M1 Pro, 8 cores) with [criterion](https://github.com/bheisler/criterion.rs):

- 8 lines: ~4.36 µs
- 1M lines: ~472 ms
- Deep nesting: ~4.26 µs

Run `cargo bench` or `cargo xtask benchmark` to verify.
