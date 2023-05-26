// This is a generated file. Not intended for manual editing.
package dev.tigr.viable.plugin.psi;

import com.intellij.psi.tree.IElementType;
import com.intellij.psi.PsiElement;
import com.intellij.lang.ASTNode;
import dev.tigr.viable.plugin.psi.impl.*;

public interface ViableTypes {

  IElementType AHEAD_RULE = new ViableElementType("AHEAD_RULE");
  IElementType BEHIND_RULE = new ViableElementType("BEHIND_RULE");
  IElementType CAPTURE_RULE = new ViableElementType("CAPTURE_RULE");
  IElementType EITHER_RULE = new ViableElementType("EITHER_RULE");
  IElementType EXPRESSION = new ViableElementType("EXPRESSION");
  IElementType LET_RULE = new ViableElementType("LET_RULE");
  IElementType MATCH_RULE = new ViableElementType("MATCH_RULE");
  IElementType NOT_RULE = new ViableElementType("NOT_RULE");
  IElementType OF_RULE = new ViableElementType("OF_RULE");
  IElementType OVER_RULE = new ViableElementType("OVER_RULE");
  IElementType RANGE_RULE = new ViableElementType("RANGE_RULE");
  IElementType STRING_RULE = new ViableElementType("STRING_RULE");
  IElementType SYMBOLS_RULE = new ViableElementType("SYMBOLS_RULE");
  IElementType TO_RULE = new ViableElementType("TO_RULE");
  IElementType VARIABLE_RULE = new ViableElementType("VARIABLE_RULE");

  IElementType AHEAD = new ViableTokenType("ahead");
  IElementType ALPHABET = new ViableTokenType("<alphabetic>");
  IElementType ALPHANUMERIC = new ViableTokenType("<alphanumeric>");
  IElementType ANY = new ViableTokenType("any");
  IElementType BACKSPACE = new ViableTokenType("<backspace>");
  IElementType BEHIND = new ViableTokenType("behind");
  IElementType BOUNDARY = new ViableTokenType("<boundary>");
  IElementType CAPTURE = new ViableTokenType("capture");
  IElementType CHAR = new ViableTokenType("<char>");
  IElementType CHARACTER = new ViableTokenType("character");
  IElementType CLOSEBRACE = new ViableTokenType("}");
  IElementType COMMENT = new ViableTokenType("comment");
  IElementType DIGIT = new ViableTokenType("<digit>");
  IElementType EITHER = new ViableTokenType("either");
  IElementType END = new ViableTokenType("<end>");
  IElementType EQUALS = new ViableTokenType("=");
  IElementType FEED = new ViableTokenType("<feed>");
  IElementType IDENTIFIER = new ViableTokenType("identifier");
  IElementType LAZY = new ViableTokenType("lazy");
  IElementType LET = new ViableTokenType("let");
  IElementType MATCH = new ViableTokenType("match");
  IElementType NEWLINE = new ViableTokenType("<newline>");
  IElementType NOT = new ViableTokenType("not");
  IElementType NULL = new ViableTokenType("<null>");
  IElementType NUMBER = new ViableTokenType("number");
  IElementType OF = new ViableTokenType("of");
  IElementType OPENBRACE = new ViableTokenType("{");
  IElementType OPTION = new ViableTokenType("option");
  IElementType OVER = new ViableTokenType("over");
  IElementType RETURN = new ViableTokenType("<return>");
  IElementType SEMICOLON = new ViableTokenType(";");
  IElementType SOME = new ViableTokenType("some");
  IElementType SPACE = new ViableTokenType("<space>");
  IElementType START = new ViableTokenType("<start>");
  IElementType STRING = new ViableTokenType("string");
  IElementType TAB = new ViableTokenType("<tab>");
  IElementType TO = new ViableTokenType("to");
  IElementType VARIABLE = new ViableTokenType("variable");
  IElementType VERTICAL = new ViableTokenType("<vertical>");
  IElementType WHITESPACELITERAL = new ViableTokenType("<whitespace>");
  IElementType WORD = new ViableTokenType("<word>");

  class Factory {
    public static PsiElement createElement(ASTNode node) {
      IElementType type = node.getElementType();
      if (type == AHEAD_RULE) {
        return new ViableAheadRuleImpl(node);
      }
      else if (type == BEHIND_RULE) {
        return new ViableBehindRuleImpl(node);
      }
      else if (type == CAPTURE_RULE) {
        return new ViableCaptureRuleImpl(node);
      }
      else if (type == EITHER_RULE) {
        return new ViableEitherRuleImpl(node);
      }
      else if (type == EXPRESSION) {
        return new ViableExpressionImpl(node);
      }
      else if (type == LET_RULE) {
        return new ViableLetRuleImpl(node);
      }
      else if (type == MATCH_RULE) {
        return new ViableMatchRuleImpl(node);
      }
      else if (type == NOT_RULE) {
        return new ViableNotRuleImpl(node);
      }
      else if (type == OF_RULE) {
        return new ViableOfRuleImpl(node);
      }
      else if (type == OVER_RULE) {
        return new ViableOverRuleImpl(node);
      }
      else if (type == RANGE_RULE) {
        return new ViableRangeRuleImpl(node);
      }
      else if (type == STRING_RULE) {
        return new ViableStringRuleImpl(node);
      }
      else if (type == SYMBOLS_RULE) {
        return new ViableSymbolsRuleImpl(node);
      }
      else if (type == TO_RULE) {
        return new ViableToRuleImpl(node);
      }
      else if (type == VARIABLE_RULE) {
        return new ViableVariableRuleImpl(node);
      }
      throw new AssertionError("Unknown element type: " + type);
    }
  }
}
