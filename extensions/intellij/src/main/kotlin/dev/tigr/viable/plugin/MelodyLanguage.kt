package dev.tigr.viable.plugin

import com.intellij.extapi.psi.PsiFileBase
import com.intellij.icons.AllIcons
import com.intellij.lang.ASTNode
import com.intellij.lang.Language
import com.intellij.lang.ParserDefinition
import com.intellij.lang.PsiParser
import com.intellij.lexer.FlexAdapter
import com.intellij.lexer.Lexer
import com.intellij.openapi.fileTypes.FileType
import com.intellij.openapi.fileTypes.LanguageFileType
import com.intellij.openapi.project.Project
import com.intellij.psi.FileViewProvider
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.tree.IFileElementType
import com.intellij.psi.tree.TokenSet
import dev.tigr.viable.plugin.parser.ViableParser
import dev.tigr.viable.plugin.psi.ViableTypes
import javax.swing.Icon

object ViableLanguage: Language("Viable")

class ViableFile(fileViewProvider: FileViewProvider): PsiFileBase(fileViewProvider, ViableLanguage) {
    override fun getFileType(): FileType = ViableLanguageFileType
    override fun toString(): String = "Viable File"
}

object ViableLanguageFileType: LanguageFileType(ViableLanguage) {
    override fun getName(): String = "Viable"
    override fun getDescription(): String = "Viable regex language"
    override fun getDefaultExtension(): String = "mdy"
    override fun getIcon(): Icon = AllIcons.FileTypes.Regexp
}

class ViableParserDefinition: ParserDefinition {
    companion object {
        private val FILE = IFileElementType(ViableLanguage)
        private val COMMENT = TokenSet.create(ViableTypes.COMMENT)
        private val STRING = TokenSet.create(ViableTypes.STRING)
    }

    override fun createLexer(project: Project?): Lexer = FlexAdapter(_ViableLexer())
    override fun createParser(project: Project?): PsiParser = ViableParser()
    override fun getFileNodeType(): IFileElementType = FILE
    override fun getCommentTokens(): TokenSet = COMMENT
    override fun getStringLiteralElements(): TokenSet = STRING
    override fun createElement(node: ASTNode?): PsiElement = ViableTypes.Factory.createElement(node)
    override fun createFile(viewProvider: FileViewProvider): PsiFile = ViableFile(viewProvider)
}
