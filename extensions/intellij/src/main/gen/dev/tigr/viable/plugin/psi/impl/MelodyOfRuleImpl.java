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

public class ViableOfRuleImpl extends ASTWrapperPsiElement implements ViableOfRule {

  public ViableOfRuleImpl(@NotNull ASTNode node) {
    super(node);
  }

  public void accept(@NotNull ViableVisitor visitor) {
    visitor.visitOfRule(this);
  }

  @Override
  public void accept(@NotNull PsiElementVisitor visitor) {
    if (visitor instanceof ViableVisitor) accept((ViableVisitor)visitor);
    else super.accept(visitor);
  }

  @Override
  @NotNull
  public ViableExpression getExpression() {
    return findNotNullChildByClass(ViableExpression.class);
  }

  @Override
  @Nullable
  public ViableOverRule getOverRule() {
    return findChildByClass(ViableOverRule.class);
  }

  @Override
  @Nullable
  public ViableRangeRule getRangeRule() {
    return findChildByClass(ViableRangeRule.class);
  }

  @Override
  @Nullable
  public PsiElement getNumber() {
    return findChildByType(NUMBER);
  }

}
