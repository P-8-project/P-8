#![no_main]
use libfuzzer_sys::fuzz_target;
use viable_compiler::{ast::types::ast::*, ast_to_regex};

fuzz_target!(|data: ViableAst| {
    drop(ast_to_regex(&data));
});
