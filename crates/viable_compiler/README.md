<p align="center">
    <img alt="Viable Logo" height="250px" src="https://user-images.githubusercontent.com/14347895/159069181-53bce5b3-a831-43f1-8c14-af6c6ed7b92b.svg">
</p>

<p align="center">
The Viable language compiler
</p>

## Install

```toml
[dependencies]
viable_compiler = "0.18.1"
```

## Usage

```rust
use viable_compiler::compiler;

let source = r#"1 to 5 of "A";"#;
let output = compiler(source);

assert_eq!(output.unwrap(), "A{1,5}");
```

## Links

- [docs.rs](https://docs.rs/viable_compiler/)
- [Language Documentation](https://yoav-lavi.github.io/viable/book/)