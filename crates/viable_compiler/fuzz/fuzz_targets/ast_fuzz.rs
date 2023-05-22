#![no_main]
use libfuzzer_sys::fuzz_target;
use viable_compiler::ast::enums::*;
use viable_compiler::to_regex;

fuzz_target!(|data: Vec<Node>| {
    drop(to_regex(&data));
});
