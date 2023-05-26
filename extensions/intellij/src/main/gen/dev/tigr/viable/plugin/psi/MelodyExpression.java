// This is a generated file. Not intended for manual editing.
package dev.tigr.viable.plugin.psi;

import java.util.List;
import org.jetbrains.annotations.*;
import com.intellij.psi.PsiElement;

public interface ViableExpression extends PsiElement {

  @Nullable
  ViableAheadRule getAheadRule();

  @Nullable
  ViableBehindRule getBehindRule();

  @Nullable
  ViableCaptureRule getCaptureRule();

  @Nullable
  ViableEitherRule getEitherRule();

  @Nullable
  ViableMatchRule getMatchRule();

  @Nullable
  ViableNotRule getNotRule();

  @Nullable
  ViableStringRule getStringRule();

  @Nullable
  ViableSymbolsRule getSymbolsRule();

  @Nullable
  ViableToRule getToRule();

  @Nullable
  ViableVariableRule getVariableRule();

}
