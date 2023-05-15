#![forbid(unsafe_code)]
#![allow(clippy::unused_unit)]
#![warn(clippy::needless_pass_by_value)]

extern crate wasm_bindgen;

use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn compiler(source: &str) -> Result<String, JsValue> {
    let output = viable_compiler::compiler(source);
    output.map_err(|error| JsValue::from(format!("Error: {}", error.message)))
}
