// This is a generated file. Not intended for manual editing.
package dev.tigr.viable.plugin.psi;

import java.util.List;
import org.jetbrains.annotations.*;
import com.intellij.psi.PsiElement;

public interface ViableAheadRule extends PsiElement {

  @NotNull
  List<ViableExpression> getExpressionList();

  @NotNull
  List<ViableLetRule> getLetRuleList();

  @NotNull
  List<ViableOfRule> getOfRuleList();

}
