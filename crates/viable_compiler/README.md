<p align="center">
    <img alt="Viable Logo" height="250px" src="https://user-images.githubusercontent.com/14347895/159065887-51a2d948-ae6f-48c4-9dd2-1ee69e76b19f.png">
</p>

<p align="center">
The Viable language compiler
</p>

## Install

```toml
[dependencies]
viable_compiler = "0.13.8"
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