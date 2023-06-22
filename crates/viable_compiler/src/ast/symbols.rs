use super::consts::{NOT, SYMBOL_NAMESPACE_DELIMITER};
use super::types::ast::{
    ViableAstNode, SpecialSymbolKind, Symbol, SymbolKind, UnicodeCategory, UnicodeCategoryKind,
};
use super::utils::first_last_inner_str;
use crate::ast::types::pest::Rule;
use crate::errors::CompilerError;
use crate::types::Result;
use pest::iterators::Pair;

pub fn symbol(pair: Pair<Rule>) -> Result<ViableAstNode> {
    let (negative, ident) = first_last_inner_str(pair)?;

    let negative = negative == NOT;

    if negative {
        match ident {
            "start" => return Err(CompilerError::NegativeStartNotAllowed),
            "end" => return Err(CompilerError::NegativeEndNotAllowed),
            _ => {}
        }
    }

    if let Some((namespace, namespaced_ident)) = ident.split_once(SYMBOL_NAMESPACE_DELIMITER) {
        return match namespace {
            "category" => unicode_category(namespaced_ident, negative),
            _ => return Err(CompilerError::UnrecognizedSymbolNamespace),
        };
    }

    let symbol_node = match ident {
        // normal symbols
        "space" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Space,
            negative,
        }),
        "newline" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Newline,
            negative,
        }),
        "vertical" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Vertical,
            negative,
        }),
        "word" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Word,
            negative,
        }),
        "digit" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Digit,
            negative,
        }),
        "whitespace" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Whitespace,
            negative,
        }),
        "boundary" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Boundary,
            negative,
        }),
        "alphabetic" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Alphabetic,
            negative,
        }),
        "alphanumeric" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Alphanumeric,
            negative,
        }),
        "return" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Return,
            negative,
        }),
        "tab" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Tab,
            negative,
        }),
        "null" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Null,
            negative,
        }),
        "feed" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Feed,
            negative,
        }),
        "char" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Char,
            negative,
        }),
        "backspace" => ViableAstNode::Symbol(Symbol {
            kind: SymbolKind::Backspace,
            negative,
        }),

        // special symbols
        "start" => ViableAstNode::SpecialSymbol(SpecialSymbolKind::Start),
        "end" => ViableAstNode::SpecialSymbol(SpecialSymbolKind::End),

        _ => return Err(CompilerError::UnrecognizedSymbol),
    };

    Ok(symbol_node)
}

#[allow(clippy::too_many_lines)]
fn unicode_category(ident: &str, negative: bool) -> Result<ViableAstNode> {
    let unicode_group_node = match ident {
        // unicode categories
        "cased_letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::CasedLetter,
            negative,
        }),
        "close_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::ClosePunctuation,
            negative,
        }),
        "connector_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::ConnectorPunctuation,
            negative,
        }),
        "control" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Control,
            negative,
        }),
        "currency_symbol" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::CurrencySymbol,
            negative,
        }),
        "dash_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::DashPunctuation,
            negative,
        }),
        "decimal_digit_number" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::DecimalDigitNumber,
            negative,
        }),
        "enclosing_mark" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::EnclosingMark,
            negative,
        }),
        "final_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::FinalPunctuation,
            negative,
        }),
        "format" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Format,
            negative,
        }),
        "initial_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::InitialPunctuation,
            negative,
        }),
        "letter_number" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::LetterNumber,
            negative,
        }),
        "letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Letter,
            negative,
        }),
        "line_separator" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::LineSeparator,
            negative,
        }),
        "lowercase_letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::LowercaseLetter,
            negative,
        }),
        "mark" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Mark,
            negative,
        }),
        "math_symbol" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::MathSymbol,
            negative,
        }),
        "modifier_letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::ModifierLetter,
            negative,
        }),
        "modifier_symbol" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::ModifierSymbol,
            negative,
        }),
        "non_spacing_mark" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::NonSpacingMark,
            negative,
        }),
        "number" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Number,
            negative,
        }),
        "open_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::OpenPunctuation,
            negative,
        }),
        "other_letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::OtherLetter,
            negative,
        }),
        "other_number" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::OtherNumber,
            negative,
        }),
        "other_punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::OtherPunctuation,
            negative,
        }),
        "other_symbol" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::OtherSymbol,
            negative,
        }),
        "other" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Other,
            negative,
        }),
        "paragraph_separator" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::ParagraphSeparator,
            negative,
        }),
        "private_use" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::PrivateUse,
            negative,
        }),
        "punctuation" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Punctuation,
            negative,
        }),
        "separator" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Separator,
            negative,
        }),
        "space_separator" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::SpaceSeparator,
            negative,
        }),
        "spacing_combining_mark" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::SpacingCombiningMark,
            negative,
        }),
        "surrogate" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Surrogate,
            negative,
        }),
        "symbol" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Symbol,
            negative,
        }),
        "titlecase_letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::TitlecaseLetter,
            negative,
        }),
        "unassigned" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::Unassigned,
            negative,
        }),
        "uppercase_letter" => ViableAstNode::UnicodeCategory(UnicodeCategory {
            kind: UnicodeCategoryKind::UppercaseLetter,
            negative,
        }),
        _ => return Err(CompilerError::UnrecognizedUnicodeCategory),
    };

    Ok(unicode_group_node)
}
