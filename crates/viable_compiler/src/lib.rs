#![forbid(unsafe_code)]
#![warn(clippy::needless_pass_by_value)]

mod ast;
pub mod errors;
mod regex;
mod utils;

use ast::to_ast;
use errors::ParseError;
use regex::to_regex;
use utils::format_line_comments;

/**
Compiles Viable source code to a regular expression.

# Errors

Returns a [`ParseError`] upon encountering a syntax error

# Example

```rust
use viable_compiler::compiler;

let source = r#"1 to 5 of "A";"#;
let output = compiler(source);

assert_eq!(output.unwrap(), "A{1,5}");
```
*/
pub fn compiler(source: &str) -> Result<String, ParseError> {
    let formatted_source = format_line_comments(source);

    let ast = to_ast(formatted_source.as_str())?;

    let output = to_regex(&ast);

    Ok(output)
}
