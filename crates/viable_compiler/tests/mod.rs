#![cfg(test)]
use viable_compiler::compiler;

#[test]
fn quantifier_test() {
    let output = compiler(
        r#"
  5 of "A";
  "#,
    );
    assert_eq!(output.unwrap(), "A{5}");
}

#[test]
fn capture_test() {
    let output = compiler(
        r#"
      capture {
        5 of "A";
        0 to 9;
      }
      "#,
    );
    assert_eq!(output.unwrap(), "(A{5}[0-9])");
}

#[test]
fn named_capture_test() {
    let output = compiler(
        r#"
      capture name {
        5 of "A";
        0 to 9;
      }
      "#,
    );
    assert_eq!(output.unwrap(), "(?<name>A{5}[0-9])");
}

#[test]
fn number_quantifier_range_test() {
    let output = compiler(
        r#"
      1 to 5 of "A";
      "#,
    );
    assert_eq!(output.unwrap(), "A{1,5}");
}

#[test]
fn uppercase_range_test() {
    let output = compiler(
        r#"
      A to Z;
      7 of A to Z;
      "#,
    );
    assert_eq!(output.unwrap(), "[A-Z][A-Z]{7}");
}

#[test]
fn lowercase_range_test() {
    let output = compiler(
        r#"
      a to z;
      8 of a to z;
      "#,
    );
    assert_eq!(output.unwrap(), "[a-z][a-z]{8}");
}

#[test]
fn open_range_expression_test() {
    let output = compiler(
        r#"
      over 4 of "a";
      "#,
    );
    assert_eq!(output.unwrap(), "a{5,}");
}

#[test]
fn start_end_test() {
    let output = compiler(
        r#"
      <start>;
      "a";
      <end>;
      "#,
    );
    assert_eq!(output.unwrap(), "^a$");
}

#[test]
fn symbol_test() {
    let output = compiler(
        r#"
      <start>;
      <char>;
      not <char>;
      <whitespace>;
      not <whitespace>;
      <newline>;
      not <newline>;
      <tab>;
      not <tab>;
      <return>;
      not <return>;
      <feed>;
      not <feed>;
      <null>;
      not <null>;
      <digit>;
      not <digit>;
      <word>;
      not <word>;
      <vertical>;
      not <vertical>;
      <alphabetic>;
      not <alphabetic>;
      <alphanumeric>;
      not <alphanumeric>;
      <space>;
      not <space>;
      <boundary>;
      not <boundary>;
      <backspace>;
      not <backspace>;
      <end>;
      "#,
    );
    assert_eq!(
        output.unwrap(),
        r"^.[^.]\s\S\n[^\n]\t[^\t]\r[^\r]\f[^\f]\0[^\0]\d\D\w\W\v[^\v][a-zA-Z][^a-zA-Z][a-zA-Z0-9][^a-zA-Z0-9] [^ ]\b\B[\b][^\b]$"
    );
}

#[test]
fn match_test() {
    let output = compiler(
        r#"
3 of match {
  5 of "A";
  0 to 9;
}"#,
    );
    assert_eq!(output.unwrap(), "(?:A{5}[0-9]){3}");
}

#[test]
fn comment_test() {
    let output = compiler(
        r#"/*  a single digit in the range of 0 to 5 */ 
    // other comment
    0 to 5; 
    match { 
      "x";
    } 
    "#,
    );
    assert_eq!(output.unwrap(), "[0-5](?:x)");
}

#[test]
fn char_test() {
    let output = compiler(
        r#"
      3 of <char>;
      "#,
    );
    assert_eq!(output.unwrap(), ".{3}");
}

#[test]
fn negative_range_test() {
    let output = compiler(
        r#"
      not 3 to 5;
      not a to z;
      "#,
    );
    assert_eq!(output.unwrap(), "[^3-5][^a-z]");
}

#[test]
fn some_test() {
    let single_output = compiler(
        r#"
      some of <char>;
      "#,
    );
    assert_eq!(single_output.unwrap(), ".+");
    let multiple_output = compiler(
        r#"
      some of "ABC";
      "#,
    );
    assert_eq!(multiple_output.unwrap(), "(?:ABC)+");
}

#[test]
fn option_test() {
    let single_output = compiler(
        r#"
      option of <char>;
      "#,
    );
    assert_eq!(single_output.unwrap(), ".?");
    let multiple_output = compiler(
        r#"
      option of "ABC";
      "#,
    );
    assert_eq!(multiple_output.unwrap(), "(?:ABC)?");
}

#[test]
fn either_test() {
    let output = compiler(
        r#"
      either {
        "first";
        "second";
        a to z;
      }
      either {
        "first";
        "second";
      }
      "#,
    );
    assert_eq!(output.unwrap(), "(?:first|second|[a-z])(?:first|second)");
}

#[test]
fn any_test() {
    let single_output = compiler(
        r#"
      any of <char>;
      "#,
    );
    assert_eq!(single_output.unwrap(), ".*");
    let multiple_output = compiler(
        r#"
        any of "ABC";
      "#,
    );
    assert_eq!(multiple_output.unwrap(), "(?:ABC)*");
}

#[test]
fn directly_quantifiable() {
    let single_output = compiler(
        r#"
      5 of <word>;
      "#,
    );
    assert_eq!(single_output.unwrap(), r"\w{5}");
}

#[test]
fn raw_test() {
    let output = compiler(
        r#"
      5 of `.*\``;
      "#,
    );
    assert_eq!(output.unwrap(), "(?:.*`){5}");
}

#[test]
fn assertion_test() {
    let output = compiler(
        r#"
      ahead {
        "a";
      }
      behind {
        "a";
      }
      not ahead {
        "a";
      }
      not behind {
        "a";
      }
      "#,
    );
    assert_eq!(output.unwrap(), "(?=a)(?<=a)(?!a)(?<!a)");
}

#[test]
fn negative_char_class_test() {
    let output = compiler(
        r#"
        not abcd;
        5 of not abcd;
        "#,
    );
    assert_eq!(output.unwrap(), "[^abcd][^abcd]{5}");
}

#[test]
fn single_quote_test() {
    let output = compiler(
        r#"
        'hello \'quoted\'';
        "#,
    );
    assert_eq!(output.unwrap(), "hello 'quoted'");
}

#[test]
fn double_quote_test() {
    let output = compiler(
        r#"
        "hello \"quoted\"";
        "#,
    );
    assert_eq!(output.unwrap(), "hello \"quoted\"");
}

#[test]
fn auto_escape_test() {
    let output = compiler(
        r#"
        "[](){}*+?|^$.-\\";
        "#,
    );
    assert_eq!(output.unwrap(), r"\[\]\(\)\{\}\*\+\?\|\^\$\.\-\\\\");
}

#[test]
fn lazy_test() {
    let output = compiler(
        r#"
        lazy any of "A";
        lazy some of "A";
        lazy option of "A";
        lazy 5 of "A";
        lazy over 5 of "A";
        lazy 5 to 6 of "A";
        "#,
    );
    assert_eq!(output.unwrap(), r"A*?A+?A??A{5}?A{6,}?A{5,6}?");
}

#[test]
fn variable_test() {
    let output = compiler(
        r#"
        let .test_variable = {
          "A";
          "B";
        }
        let .second_test_variable = {
          "C";
        }
        .test_variable;
        .second_test_variable;
        "#,
    );
    assert_eq!(output.unwrap(), r"ABC");
}
