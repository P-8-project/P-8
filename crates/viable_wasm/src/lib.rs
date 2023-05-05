use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn compiler(source: &str) -> Result<String, JsValue> {
    Ok(viable::compiler(&source).expect_throw("Encountered a parsing error"))
}
