// This is a generated file. Not intended for manual editing.
package dev.tigr.viable.plugin.psi.impl;

import java.util.List;
import org.jetbrains.annotations.*;
import com.intellij.lang.ASTNode;
import com.intellij.psi.PsiElement;
import com.intellij.psi.PsiElementVisitor;
import com.intellij.psi.util.PsiTreeUtil;
import static dev.tigr.viable.plugin.psi.ViableTypes.*;
import com.intellij.extapi.psi.ASTWrapperPsiElement;
import dev.tigr.viable.plugin.psi.*;

public class ViableExpressionImpl extends ASTWrapperPsiElement implements ViableExpression {

  public ViableExpressionImpl(@NotNull ASTNode node) {
    super(node);
  }

  public void accept(@NotNull ViableVisitor visitor) {
    visitor.visitExpression(this);
  }

  @Override
  public void accept(@NotNull PsiElementVisitor visitor) {
    if (visitor instanceof ViableVisitor) accept((ViableVisitor)visitor);
    else super.accept(visitor);
  }

  @Override
  @Nullable
  public ViableAheadRule getAheadRule() {
    return findChildByClass(ViableAheadRule.class);
  }

  @Override
  @Nullable
  public ViableBehindRule getBehindRule() {
    return findChildByClass(ViableBehindRule.class);
  }

  @Override
  @Nullable
  public ViableCaptureRule getCaptureRule() {
    return findChildByClass(ViableCaptureRule.class);
  }

  @Override
  @Nullable
  public ViableEitherRule getEitherRule() {
    return findChildByClass(ViableEitherRule.class);
  }

  @Override
  @Nullable
  public ViableMatchRule getMatchRule() {
    return findChildByClass(ViableMatchRule.class);
  }

  @Override
  @Nullable
  public ViableNotRule getNotRule() {
    return findChildByClass(ViableNotRule.class);
  }

  @Override
  @Nullable
  public ViableStringRule getStringRule() {
    return findChildByClass(ViableStringRule.class);
  }

  @Override
  @Nullable
  public ViableSymbolsRule getSymbolsRule() {
    return findChildByClass(ViableSymbolsRule.class);
  }

  @Override
  @Nullable
  public ViableToRule getToRule() {
    return findChildByClass(ViableToRule.class);
  }

  @Override
  @Nullable
  public ViableVariableRule getVariableRule() {
    return findChildByClass(ViableVariableRule.class);
  }

}
