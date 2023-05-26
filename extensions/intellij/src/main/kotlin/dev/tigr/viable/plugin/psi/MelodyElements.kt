package dev.tigr.viable.plugin.psi

import com.intellij.psi.tree.IElementType
import dev.tigr.viable.plugin.ViableLanguage

class ViableTokenType(debugName: String): IElementType(debugName, ViableLanguage)
class ViableElementType(debugName: String): IElementType(debugName, ViableLanguage)
