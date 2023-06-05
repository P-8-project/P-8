const { compiler } = require("viablec");

const MARKER = "viable";

module.exports = () => ({
  visitor: {
    TemplateLiteral(literal) {
      let hasViableMarker =
        literal.node.leadingComments?.find?.(
          (comment) => comment.value.trim() === MARKER
        ) !== undefined;
      if (hasViableMarker) {
        literal.node.leadingComments = literal.node.leadingComments.filter(
          (comment) => comment.value.trim() !== MARKER
        );
        literal.traverse({
          TemplateElement(element) {
            const source = compiler(element.node.value.raw.replace(MARKER, ""));
            element.replaceWithSourceString(JSON.stringify(source));
          },
        });
      }
    },
    StringLiteral(literal) {
      let hasViableMarker =
        literal.node.leadingComments?.find?.(
          (comment) => comment.value.trim() === MARKER
        ) !== undefined;

      if (hasViableMarker) {
        literal.node.leadingComments = literal.node.leadingComments.filter(
          (comment) => comment.value.trim() !== MARKER
        );
        const source = compiler(literal.node.value);
        literal.replaceWithSourceString(JSON.stringify(source));
      }
    },
  },
});
