use super::consts::{LAZY, NOT};
use super::enums::*;
use super::error_messages::ErrorMessage;
use super::ident_parser::{IdentParser, Rule};
use super::utils::{
    alphabetic_first_char, first_inner, first_last_inner_str, last_inner, nth_inner, to_char,
    unquote_escape_literal, unquote_escape_raw,
};
use crate::errors::ParseError;
use pest::iterators::Pairs;
use pest::{iterators::Pair, Parser};
use std::collections::HashMap;

pub fn to_ast(source: &str) -> Result<ViableAst, ParseError> {
    if source.is_empty() {
        return Ok(ViableAst::Empty);
    }

    let mut pairs = IdentParser::parse(Rule::root, source).map_err(|error| ParseError {
        message: error.to_string(),
    })?;

    let root_statements = pairs
        .next()
        .ok_or_else(|| ParseError::from(ErrorMessage::MissingRootNode))?;

    pairs_to_ast(root_statements.into_inner())
}

pub fn pairs_to_ast(pairs: Pairs<Rule>) -> Result<ViableAst, ParseError> {
    let mut variables: HashMap<String, ViableAst> = HashMap::new();
    let mut nodes = Vec::new();

    for pair in pairs {
        let node = create_ast_node(pair, &mut variables)?;
        nodes.push(node);
    }

    Ok(ViableAst::Root(nodes))
}

fn create_ast_node(
    pair: Pair<Rule>,
    variables: &mut HashMap<String, ViableAst>,
) -> Result<ViableAstNode, ParseError> {
    let node = match pair.as_rule() {
        Rule::raw => ViableAstNode::Atom(unquote_escape_raw(&pair)),
        Rule::literal => ViableAstNode::Atom(unquote_escape_literal(&pair)),
        Rule::symbol => symbol(pair)?,
        Rule::range => range(pair)?,
        Rule::quantifier => quantifier(pair, variables)?,
        Rule::group => group(pair)?,
        Rule::assertion => assertion(pair)?,
        Rule::negative_char_class => negative_char_class(&pair)?,
        Rule::variable_invocation => variable_invocation(&pair, variables)?,
        Rule::variable_declaration => variable_declaration(pair, variables)?,
        Rule::EOI => ViableAstNode::Skip,
        _ => return Err(ErrorMessage::UnrecognizedSyntax.into()),
    };

    Ok(node)
}

fn symbol(pair: Pair<Rule>) -> Result<ViableAstNode, ParseError> {
    let (negative, ident) = first_last_inner_str(pair)?;

    let negative = negative == NOT;

    if negative {
        match ident {
            "start" => return Err(ErrorMessage::NegativeStartNotAllowed.into()),
            "end" => return Err(ErrorMessage::NegativeEndNotAllowed.into()),
            _ => {}
        }
    }

    let symbol_node = match ident {
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

        "start" => ViableAstNode::SpecialSymbol(SpecialSymbol::Start),
        "end" => ViableAstNode::SpecialSymbol(SpecialSymbol::End),

        _ => return Err(ErrorMessage::UnrecognizedSymbol.into()),
    };

    Ok(symbol_node)
}

fn range(pair: Pair<Rule>) -> Result<ViableAstNode, ParseError> {
    let (first, end) = first_last_inner_str(pair.clone())?;
    let negative = first == NOT;
    let start = if negative {
        nth_inner(pair, 1)
            .ok_or_else(|| ParseError::from(ErrorMessage::MissingNode))?
            .as_str()
    } else {
        first
    };
    let range_node = if alphabetic_first_char(start)? {
        ViableAstNode::Range(Range::AsciiRange(AsciiRange {
            negative,
            start: to_char(start)?,
            end: to_char(end)?,
        }))
    } else {
        ViableAstNode::Range(Range::NumericRange(NumericRange {
            negative,
            start: to_char(start)?,
            end: to_char(end)?,
        }))
    };

    Ok(range_node)
}

fn quantifier(
    pair: Pair<Rule>,
    variables: &mut HashMap<String, ViableAst>,
) -> Result<ViableAstNode, ParseError> {
    let quantity = first_inner(pair.clone())?;
    let kind = first_inner(quantity.clone())?;
    let expression = create_ast_node(last_inner(pair)?, variables)?;

    let expression = match expression {
        ViableAstNode::Group(group) => Expression::Group(group),
        ViableAstNode::Atom(atom) => Expression::Atom(atom),
        ViableAstNode::Range(range) => Expression::Range(range),
        ViableAstNode::Symbol(symbol) => Expression::Symbol(symbol),
        ViableAstNode::NegativeCharClass(class) => Expression::NegativeCharClass(class),

        // unexpected nodes
        ViableAstNode::SpecialSymbol(_) => {
            return Err(ErrorMessage::UnexpectedSpecialSymbolInQuantifier.into())
        }
        ViableAstNode::Quantifier(_) => {
            return Err(ErrorMessage::UnexpectedQuantifierInQuantifier.into())
        }
        ViableAstNode::Assertion(_) => {
            return Err(ErrorMessage::UnexpectedAssertionInQuantifier.into())
        }
        ViableAstNode::VariableInvocation(_) => {
            return Err(ErrorMessage::UnexpectedVariableInvocationInQuantifier.into())
        }
        ViableAstNode::Skip => return Err(ErrorMessage::UnexpectedSkippedNodeInQuantifier.into()),
    };

    let lazy = quantity.as_str().starts_with(LAZY);

    let quantifier_node = match kind.as_rule() {
        Rule::amount => ViableAstNode::Quantifier(Quantifier {
            kind: QuantifierKind::Amount(kind.as_str().to_owned()),
            lazy,
            expression: Box::new(expression),
        }),
        Rule::over => {
            let raw_amount = last_inner(kind)?.as_str().to_owned();
            let amount = raw_amount
                .parse::<usize>()
                .map_err(|_| ParseError::from(ErrorMessage::CouldNotParseAnAmount))?
                .checked_add(1)
                .ok_or_else(|| ParseError::from(ErrorMessage::CouldNotParseAnAmount))?;
            ViableAstNode::Quantifier(Quantifier {
                kind: QuantifierKind::Over(amount),
                lazy,
                expression: Box::new(expression),
            })
        }
        Rule::option => ViableAstNode::Quantifier(Quantifier {
            kind: QuantifierKind::Option,
            lazy,
            expression: Box::new(expression),
        }),
        Rule::any => ViableAstNode::Quantifier(Quantifier {
            kind: QuantifierKind::Any,
            lazy,
            expression: Box::new(expression),
        }),
        Rule::some => ViableAstNode::Quantifier(Quantifier {
            kind: QuantifierKind::Some,
            lazy,
            expression: Box::new(expression),
        }),

        Rule::quantifier_range => {
            let (start, end) = first_last_inner_str(kind)?;
            ViableAstNode::Quantifier(Quantifier {
                kind: QuantifierKind::Range {
                    start: start.to_owned(),
                    end: end.to_owned(),
                },
                lazy,
                expression: Box::new(expression),
            })
        }

        _ => return Err(ErrorMessage::UnrecognizedSyntax.into()),
    };

    Ok(quantifier_node)
}

fn group(pair: Pair<Rule>) -> Result<ViableAstNode, ParseError> {
    let declaration = first_inner(pair.clone())?;

    let kind = first_inner(declaration.clone())?.as_str();

    let kind = match kind {
        "either" => GroupKind::Either,
        "capture" => GroupKind::Capture,
        "match" => GroupKind::Match,

        _ => return Err(ErrorMessage::UnrecognizedGroup.into()),
    };

    let ident = nth_inner(declaration, 1).map(|ident| ident.as_str().trim().to_owned());

    if ident.is_some() && kind != GroupKind::Capture {
        return Err(ErrorMessage::UnexpectedIdentifierForNonCaptureGroup.into());
    }

    let block = last_inner(pair)?;

    let group_node = ViableAstNode::Group(Group {
        ident,
        kind,
        statements: Box::new(pairs_to_ast(block.into_inner())?),
    });

    Ok(group_node)
}

fn assertion(pair: Pair<Rule>) -> Result<ViableAstNode, ParseError> {
    let assertion_declaration = first_inner(pair.clone())?;

    let (negative, kind) = first_last_inner_str(assertion_declaration)?;

    let negative = negative == NOT;

    let kind = match kind {
        "ahead" => AssertionKind::Ahead,
        "behind" => AssertionKind::Behind,
        _ => return Err(ErrorMessage::UnrecognizedAssertion.into()),
    };

    let block = last_inner(pair)?;

    let assertion_node = ViableAstNode::Assertion(Assertion {
        kind,
        negative,
        statements: Box::new(pairs_to_ast(block.into_inner())?),
    });

    Ok(assertion_node)
}

fn negative_char_class(pair: &Pair<Rule>) -> Result<ViableAstNode, ParseError> {
    let class = last_inner(pair.clone())?;
    let negative_char_class_node = ViableAstNode::NegativeCharClass(class.as_str().to_owned());
    Ok(negative_char_class_node)
}

fn variable_invocation(
    pair: &Pair<Rule>,
    variables: &mut HashMap<String, ViableAst>,
) -> Result<ViableAstNode, ParseError> {
    let identifier = last_inner(pair.clone())?;
    let statements = match variables.get(identifier.as_str()) {
        Some(statements) => statements.clone(),
        None => return Err(ErrorMessage::UninitializedVariable.into()),
    };
    let variable_invocation_node = ViableAstNode::VariableInvocation(VariableInvocation {
        statements: Box::new(statements),
    });
    Ok(variable_invocation_node)
}

fn variable_declaration(
    pair: Pair<Rule>,
    variables: &mut HashMap<String, ViableAst>,
) -> Result<ViableAstNode, ParseError> {
    let identifier = first_inner(pair.clone())?;
    let statements = last_inner(pair)?;
    variables.insert(
        identifier.as_str().trim().to_owned(),
        pairs_to_ast(statements.into_inner())?,
    );
    Ok(ViableAstNode::Skip)
}
