<p align="center">
  <img width="520" alt="code example" src="https://user-images.githubusercontent.com/14347895/154124756-ddbd3c84-f8b2-45bd-b624-2c510482c4e2.png">
</p>

Viable is a language designed to compile to and maintain a 1-1 relationship with regular expressions, while being more readable and maintainable.

The current goal is supporting the JavaScript implementation of regular expressions.

⚠️ Viable is very new and is unstable at the moment ⚠️

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
/(?:na){16}(?:\sbatman){2}/
```

### Twitter Hashtag

```rust
"#";
some of <word>;

// #viable
```

Turns into

```regex
/#\w+/
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
/\w+\s1\d{2}/
```

### Indented Code (2 spaces)

```rust
some of match {
  2 of <space>;
}

some of char;
";";

//  let value = 5;
```

Turns into

```regex
/(?:\s{2})+.+;/
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

## CLI Usage

```sh
viable [OPTIONS] <PATH>

OPTIONS:
  -f, --file <FILE>    write to an output file
  -n, --no-color       print output with no color
  -V, --version        print version information
  -h, --help           print help information
```

## Keywords

- `of` - used after a number or a range and before a sequence to be matched, e.g. `5 of "A";`, equivalent to regex `{5}`
- `to` - used to create a range (either as a quantifier or as a character range), e.g. `5 to 9`, equivalent to regex `{5,9}` if before an `of` or `[5-9]` otherwise
- `capture` - used to open a `capture` or named `capture` block, equivalent to regex `(...)`
- `match` - used to open a `match` block, equivalent to regex `(?:...)`
- `start` - matches the start of the string, equivalent to regex `^`
- `end` - matches the end of the string, equivalent to regex `$`
- `char` - matches a single character, equivalent to regex `.`
- `some` - used with `of` to express 1 or more of a pattern, equivalent to regex `+`
- `over` - used with `of` to express more than an amount of a pattern, equivalent to regex `{6,}` (assuming `over 5 of ...`)

## Symbols

- `<space>` - equivalent to regex `\s`
- `<newline>` - equivalent to regex `\n`
- `<tab>` - equivalent to regex `\t`
- `<return>` - equivalent to regex `\r`
- `<feed>` - equivalent to regex `\f`
- `<null>` - equivalent to regex `\0`
- `<digit>` - equivalent to regex `\d`
- `<vertical>` - equivalent to regex `\v`
- `<word>` - equivalent to regex `\w`

## Concepts

- `"..."` or `'...'` - used to mark a literal part of the match

## Extras

- `//` - used to mark comments

## File Extension

The Viable file extension is `.mdy`

## Crates

- `viable_compiler` - The Viable compiler [📦](https://crates.io/crates/viable_compiler) [📖](https://docs.rs/viable_compiler/)
- `viable_cli` - A CLI wrapping the Viable compiler [📦](https://crates.io/crates/viable_cli) [📖](https://docs.rs/crate/viable_cli/0.1.1)
- `viable_wasm` - WASM binding for the Viable compiler

## Extensions

- [VSCode](https://marketplace.visualstudio.com/items?itemName=yoavlavi.viable)

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
| `<space>;`                          | `\s`                  | ✅          |
| `A to Z;`                           | `[A-Z]`               | ✅          |
| `a to z;`                           | `[a-z]`               | ✅          |
| `0 to 9;`                           | `[0-9]`               | ✅          |
| `// comment`                        |                       | ✅          |
| `start;`                            | `^`                   | ✅          |
| `end;`                              | `$`                   | ✅          |
| `<newline>;`                        | `\n`                  | ✅          |
| `<tab>;`                            | `\t`                  | ✅          |
| `<return>;`                         | `\r`                  | ✅          |
| `<feed>;`                           | `\f`                  | ✅          |
| `<null>;`                           | `\0`                  | ✅          |
| `<digit>;`                          | `\d`                  | ✅          |
| `<vertical>;`                       | `\v`                  | ✅          |
| `<word>;`                           | `\w`                  | ✅          |
| `"...";` (raw)                      | ...                   | ✅          |
| `'...';` (raw)                      | ...                   | ✅          |
| `'\'';`                             | `'`                   | ✅          |
| `"\"";`                             | `"`                   | ✅          |
| support non alphanumeric characters |                       | ✅          |
| output to file                      |                       | ✅          |
| no color output                     |                       | ✅          |
| `char`                              | `.`                   | ✅          |
| `some of`                           | `+`                   | ✅          |
| syntax highlighting extension       |                       | ✅          |
| `over 5 of "A";`                    | `A{6,}`               | ✅          |
| WASM binding                        |                       | ✅          |
| Rust crate                          |                       | ✅          |
| enforce group close                 |                       | 🐣          |
| tests                               |                       | 🐣          |
| `not <space>;`                      | `\S`                  | ❌          |
| `not <digit>;`                      | `\D`                  | ❌          |
| `not <word>;`                       | `\W`                  | ❌          |
| `<backspace>`                       | `[\b]`                | ❌          |
| file watcher                        |                       | ❌          |
| nested groups                       | `(...(...))`          | ❌          |
| multiple ranges                     | `[a-zA-Z0-9]`         | ❌          |
| general cleanup and modules         |                       | ❌          |
| auto escape for non Viable patterns |                       | ❌          |
| TS / JS build step                  |                       | ❌          |
| more robust parsing mechanism (ast) |                       | ❌          |
| `not "A";`                          | `[^A]`                | ❔          |
| `flags: global, multiline, ...`     | `/.../gm...`          | ❔          |
| `/* comment */`                     |                       | ❔          |
| `maybe of`                          | `?`                   | ❔          |
| `maybe some of`                     | `*`                   | ❔          |
| `either of ..., ...`                | `\|`                  | ❔          |
| `any of "a", "b", "c"`              | `[abc]`               | ❔          |
| `... not before ...`                | `...(?!...)`          | ❔          |
| `... not after ...`                 | `...(?<!...)`         | ❔          |
| `... before ...`                    | `...(?=...)`          | ❔          |
| `... after ...`                     | `...(?<=...)`         | ❔          |
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
| (?)                                 | <code>$`</code>       | ❔          |
| (?)                                 | `$&`                  | ❔          |
| (?)                                 | `x20`                 | ❔          |
| (?)                                 | `x{06fa}`             | ❔          |
| variables / macros                  |                       | ❓          |
| regex optimization                  |                       | ❓          |
| standard library / patterns         |                       | ❓          |
| reverse compiler                    |                       | ❓          |
