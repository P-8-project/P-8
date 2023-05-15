<h1 align="center">
  Viable
</h1>

<p align="center">
  <img width="520" alt="code example" src="https://user-images.githubusercontent.com/14347895/154124756-ddbd3c84-f8b2-45bd-b624-2c510482c4e2.png">
</p>


<p align="center">
  <a href="https://github.com/yoav-lavi/viable/actions/workflows/rust.yml">
    <img alt="GitHub Workflow Status (branch)" src="https://img.shields.io/github/workflow/status/yoav-lavi/viable/Rust/main">
  </a>
  <a href="https://crates.io/crates/viable_compiler">
    <img alt="Crates.io" src="https://img.shields.io/crates/v/viable_compiler?label=compiler">
  </a>
  <a href="https://crates.io/crates/viable_cli">
    <img alt="Crates.io" src="https://img.shields.io/crates/v/viable_cli?label=cli">
  </a>
  <a href="https://github.com/yoav-lavi/viable/blob/main/LICENSE">
    <img alt="Crates.io" src="https://img.shields.io/crates/l/viable_compiler">
  </a>
  <a href="https://viable-playground.vercel.app">
    <img alt="viable playground" src="https://img.shields.io/badge/viable-playground-brightgreen"
  </a>
  <a href="https://yoav-lavi.github.io/viable/docs/intro">
    <img alt="viable playground" src="https://img.shields.io/badge/viable-docs-blue">
  </a>
  <a href="https://marketplace.visualstudio.com/items?itemName=yoavlavi.viable">
    <img alt="Visual Studio Code Marketplace Version" src="https://img.shields.io/visual-studio-marketplace/v/yoavlavi.viable?label=vscode%20extension">
  </a>
</p>

Viable is a language designed to compile to and maintain a 1-1 relationship with regular expressions, while being more readable and maintainable.

The current goal is supporting the JavaScript implementation of regular expressions.

## Examples

Note: these are for the currently supported syntax and may change

### Batman Theme

```rust
16 of "na";

2 of match {
  <space>;
  "batman";
}

// 🦇🦸‍♂️
```

Turns into

```regex
(?:na){16}(?: batman){2}
```

### Twitter Hashtag

```rust
"#";
some of <word>;

// #viable
```

Turns into

```regex
#\w+
```

### Introductory Courses

```rust
some of <word>;
<space>;
"1";
2 of <digit>;

// classname 1xx
```

Turns into

```regex
\w+ 1\d{2}
```

### Indented Code (2 spaces)

```rust
some of match {
  2 of <space>;
}

some of <char>;
";";

// let value = 5;
```

Turns into

```regex
(?: {2})+.+;
```

### Semantic Versions

```rust
<start>;

option of "v";

capture major {
  some of <digit>;
}

".";

capture minor {
  some of <digit>;
}

".";

capture patch {
  some of <digit>;
}

<end>;

// v1.0.0
```

Turns into

```regex
^v?(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)$
```

## Playground

You can try Viable in your browser using the [playground](https://viable-playground.vercel.app/)

## Documentation

Read the documentation [here](https://yoav-lavi.github.io/viable/)

## Install

### Cargo

```sh
cargo install viable_cli
```

### From Source

```sh
git clone https://github.com/yoav-lavi/viable.git
cd viable
cargo install --path crates/viable_cli
```
### Binary

- macOS binaries (aarch64 and x86_64) can be downloaded from the [release page](https://github.com/yoav-lavi/viable/releases)

### Community

- [Arch Linux](https://aur.archlinux.org/packages/viable) (maintained by [@ilai-deutel](https://github.com/ilai-deutel))
  <details><summary>Installation instructions</summary>
  
  1. Installation with an AUR helper, for instance using `paru`:
  
     ```bash
     paru -Syu viable
     ```
  
  2. Install manually with `makepkg`:
  
     ```bash
     git clone https://aur.archlinux.org/viable.git
     cd viable
     makepkg -si
     ```
  
  </details>

- NixOS (soon, see [PR](https://github.com/NixOS/nixpkgs/pull/160985)) (maintained by [@jyooru](https://github.com/jyooru))

## CLI Usage

```sh
viable [OPTIONS] [INPUT_FILE_PATH]

ARGS:
    <INPUT_FILE_PATH>    Read from a file

OPTIONS:
    -h, --help                         Print help information
    -n, --no-color                     Print output with no color
    -o, --output <OUTPUT_FILE_PATH>    Write to a file
    -r, --repl                         Start the Viable REPL
    -V, --version                      Print version information
```

## Changelog

See the changelog [here](https://github.com/yoav-lavi/viable/blob/main/CHANGELOG.md) or in the [release page](https://github.com/yoav-lavi/viable/releases)

## Keywords

- `of` - used after a number or a range and before a sequence to be matched, e.g. `5 of "A";`, equivalent to regex `{5}`
- `to` - used to create a range (either as a quantifier or as a character range), e.g. `5 to 9`, equivalent to regex `{5,9}` if before an `of` or `[5-9]` otherwise. `not` can be used before a range to create a negative range, e.g. `[^1-3]`
- `capture` - used to open a `capture` or named `capture` block, equivalent to regex `(...)`
- `match` - used to open a `match` block, equivalent to regex `(?:...)`
- `some` - used with `of` to express 1 or more of a pattern, equivalent to regex `+`
- `over` - used with `of` to express more than an amount of a pattern, equivalent to regex `{6,}` (assuming `over 5 of ...`)
- `option` - used with `of` to express 0 or 1 of a pattern, equivalent to regex `?`
- `either` - used to open an `either` block, equivalent to regex `(?:...|...)`
- `any` - used with `of` to express 0 or more of a pattern, equivalent to regex `*`
- `ahead` - used to open an `ahead` block, equivalent to regex `(?=...)`. use after an expression
- `not ahead` - used to open a `not ahead` block, equivalent to regex `(?!...)`. use after an expression
- `behind` - used to open an `behind` block, equivalent to regex `(?<=...)`. use before an expression
- `not behind` - used to open a `not behind` block, equivalent to regex `(?<!...)`. use before an expression

## Symbols

- `<start>` - matches the start of the string, equivalent to regex `^`
- `<end>` - matches the end of the string, equivalent to regex `$`
- `<char>` - matches a single character, equivalent to regex `.`
- `<whitespace>` - equivalent to regex `\s`
- `not <whitespace>` - equivalent to regex `\S`
- `<newline>` - equivalent to regex `\n`
- `<tab>` - equivalent to regex `\t`
- `<return>` - equivalent to regex `\r`
- `<feed>` - equivalent to regex `\f`
- `<null>` - equivalent to regex `\0`
- `<digit>` - equivalent to regex `\d`
- `not <digit>` - equivalent to regex `\D`
- `<vertical>` - equivalent to regex `\v`
- `<word>` - equivalent to regex `\w`
- `not <word>` - equivalent to regex `\W`
- `<alphabet>` - equivalent to regex `[a-zA-Z]`
- `<boundary>` - equivalent to regex `\b`
- `<backspace>` - equivalent to regex `[\b]`

## Literals

- `"..."` or `'...'` - used to mark a literal part of the match. Viable will automatically escape characters as needed. Quotes (of the same kind surrounding the literal) should be escaped

## Raw

- <code>\`...\`</code> - added directly to the output without any escaping

## Extras

- `/* ... */`, `// ...` - used to mark comments (note: `// ...` comments must be on separate line)

## File Extension

The Viable file extension is `.mdy`

## Crates

- `viable_compiler` - The Viable compiler [📦](https://crates.io/crates/viable_compiler) [📖](https://docs.rs/viable_compiler/)
- `viable_cli` - A CLI wrapping the Viable compiler [📦](https://crates.io/crates/viable_cli) [📖](https://docs.rs/crate/viable_cli)
- `viable_wasm` - WASM binding for the Viable compiler

## Extensions

- [VSCode](https://marketplace.visualstudio.com/items?itemName=yoavlavi.viable)


## Performance

Last measured on [v0.12.0](https://github.com/yoav-lavi/viable/releases/tag/v0.12.0)

Measured on an 8 core 2021 MacBook Pro 14-inch, Apple M1 Pro using [criterion](https://github.com/bheisler/criterion.rs):

- 8 lines:
  ```
  compiler/normal (8 lines)                                                                               
                          time:   [4.4636 us 4.4911 us 4.5162 us]
  slope  [4.4636 us 4.5162 us] R^2            [0.9978146 0.9979425]
  mean   [4.4456 us 4.5138 us] std. dev.      [30.807 ns 78.087 ns]
  median [4.4405 us 4.5161 us] med. abs. dev. [1.7263 ns 102.81 ns]
  ```

- 1M lines:
  ```
  compiler/long input (1M lines)                                                                          
                          time:   [414.41 ms 418.28 ms 421.92 ms]
  mean   [414.41 ms 421.92 ms] std. dev.      [3.9838 ms 7.6492 ms]
  median [412.74 ms 424.18 ms] med. abs. dev. [613.87 us 11.100 ms]
  ```

- Deeply nested:

  ```
  compiler/deeply nested  
                          time:   [5.0808 us 5.1471 us 5.2011 us]
  slope  [5.0808 us 5.2011 us] R^2            [0.9926848 0.9936608]
  mean   [5.0739 us 5.1884 us] std. dev.      [64.329 ns 110.75 ns]
  median [5.0313 us 5.2249 us] med. abs. dev. [6.1076 ns 151.57 ns]
  ```

To reproduce, run `cargo benchmark`

## Feature Status

✅ - Implemented

🐣 - Partially implemented

❌ - Not implemented

❓ - Unclear whether this will be implemented

❔ - Unclear what the syntax will be

| Viable                              | Regex                 | Status      |
| ----------------------------------- | --------------------- | ----------- |
| `5 of "hello";`                     | `(?:hello){5}`        | ✅          |
| `5 to 7 of "A";`                    | `A{5,7}`              | ✅          |
| `capture { ... }`                   | `(...)`               | ✅          |
| `capture name { ... }`              | `(?<name>...)`        | ✅          |
| `match { ... }`                     | `(?:...)`             | ✅          |
| `<whitespace>;`                     | `\s`                  | ✅          |
| `<space>;`                          | ` `                   | ✅          |
| `A to Z;`                           | `[A-Z]`               | ✅          |
| `a to z;`                           | `[a-z]`               | ✅          |
| `0 to 9;`                           | `[0-9]`               | ✅          |
| `not a to z;`                       | `[^a-z]`              | ✅          |
| `<start>;`                          | `^`                   | ✅          |
| `<end>;`                            | `$`                   | ✅          |
| `<newline>;`                        | `\n`                  | ✅          |
| `<tab>;`                            | `\t`                  | ✅          |
| `<return>;`                         | `\r`                  | ✅          |
| `<feed>;`                           | `\f`                  | ✅          |
| `<null>;`                           | `\0`                  | ✅          |
| `<digit>;`                          | `\d`                  | ✅          |
| `<vertical>;`                       | `\v`                  | ✅          |
| `<word>;`                           | `\w`                  | ✅          |
| `<alphabet>;`                       | `[a-zA-Z]`            | ✅          |
| `"...";` (literal)                  | `...`                 | ✅          |
| `'...';` (literal)                  | `...`                 | ✅          |
| <code>\`...\`;</code> (raw)         | `...`                 | ✅          |
| `'\'';`                             | `'`                   | ✅          |
| `"\"";`                             | `"`                   | ✅          |
| <code>`\\\``;</code>                | <code>\`</code>       | ✅          |
| support non alphanumeric characters |                       | ✅          |
| output to file                      |                       | ✅          |
| no color output                     |                       | ✅          |
| `<char>`                            | `.`                   | ✅          |
| `some of`                           | `+`                   | ✅          |
| syntax highlighting extension       |                       | ✅          |
| `over 5 of "A";`                    | `A{6,}`               | ✅          |
| `not <digit>;`                      | `\D`                  | ✅          |
| `not <whitespace>;`                 | `\S`                  | ✅          |
| `not <word>;`                       | `\W`                  | ✅          |
| WASM binding                        |                       | ✅          |
| Rust crate                          |                       | ✅          |
| `option of`                         | `?`                   | ✅          |
| `any of`                            | `*`                   | ✅          |
| `either { ...; ...; }`              | `(?:...\|...)`        | ✅          |
| tests                               |                       | ✅          |
| auto escape for literals            |                       | ✅          |
| `ahead { ... }`                     | `(?=...)`             | ✅          |
| `behind { ... }`                    | `(?<=...)`            | ✅          |
| `not ahead { ... }`                 | `(?!...)`             | ✅          |
| `not behind { ... }`                | `(?<!...)`            | ✅          |
| `/* comment */`                     |                       | ✅          |
| enforce group close                 |                       | ✅          |
| nested groups                       | `(...(...))`          | ✅          |
| general cleanup and modules         |                       | ✅          |
| more robust parsing mechanism (ast) |                       | ✅          |
| `<backspace>`                       | `[\b]`                | ✅          |
| `<boundary>`                        | `\b`                  | ✅          |
| `// comment`                        |                       | ✅          |
| `not "A";`                          | `[^A]`                | 🐣          |
| file watcher                        |                       | ❌          |
| multiple ranges                     | `[a-zA-Z0-9]`         | ❌          |
| TS / JS build step                  |                       | ❌          |
| multiline groups in REPL            |                       | ❌          |
| `flags: global, multiline, ...`     | `/.../gm...`          | ❔          |
| `any of "a", "b", "c"`              | `[abc]`               | ❔          |
| (?)                                 | `*?`                  | ❔          |
| (?)                                 | `\#`                  | ❔          |
| (?)                                 | `\k<name>`            | ❔          |
| (?)                                 | `\p{...}`             | ❔          |
| (?)                                 | `\P{...}`             | ❔          |
| (?)                                 | `\uYYYY`              | ❔          |
| (?)                                 | `\xYY`                | ❔          |
| (?)                                 | `\ddd`                | ❔          |
| (?)                                 | `\cY`                 | ❔          |
| (?)                                 | `\b`                  | ❔          |
| (?)                                 | `\B`                  | ❔          |
| (?)                                 | `$1`                  | ❔          |
| (?)                                 | <code>$\`</code>      | ❔          |
| (?)                                 | `$&`                  | ❔          |
| (?)                                 | `x20`                 | ❔          |
| (?)                                 | `x{06fa}`             | ❔          |
| variables / macros                  |                       | ❓          |
| regex optimization                  |                       | ❓          |
| standard library / patterns         |                       | ❓          |
| reverse compiler                    |                       | ❓          |
