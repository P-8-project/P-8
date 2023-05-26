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

public class ViableNotRuleImpl extends ASTWrapperPsiElement implements ViableNotRule {

  public ViableNotRuleImpl(@NotNull ASTNode node) {
    super(node);
  }

  public void accept(@NotNull ViableVisitor visitor) {
    visitor.visitNotRule(this);
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
  public ViableSymbolsRule getSymbolsRule() {
    return findChildByClass(ViableSymbolsRule.class);
  }

}
