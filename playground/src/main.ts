import * as monaco from "monaco-editor";
import editorWorker from "monaco-editor/esm/vs/editor/editor.worker?worker";
// build from ../crates/viable_wasm included manually due to issues with wasm-bindgen (similar to https://github.com/rustwasm/wasm-bindgen/issues/113)
import init, { compiler } from "./wasm/viable_wasm";

declare global {
  interface Window {
    MonacoEnvironment: {};
  }
}

window.MonacoEnvironment = {
  getWorker() {
    return new editorWorker();
  },
};

const initEditors = () => {
  const editorTarget = document.getElementById("editor-container");
  const outputTarget = document.getElementById("output-container");

  if (!editorTarget || !outputTarget) {
    return;
  }

  const initialValue = `16 of "na";

  2 of match {
    <space>;
    "batman";
  }`;

  const editor = monaco.editor.create(editorTarget, {
    value: initialValue,
    theme: "vs-dark",
    automaticLayout: true,
    minimap: { enabled: false },
    wordWrap: "on",
    wrappingIndent: "indent",
    fontFamily: "'Fira Code', monospace",
    fontLigatures: true,
  });

  const output = monaco.editor.create(outputTarget, {
    readOnly: true,
    value: ``,
    theme: "vs-dark",
    automaticLayout: true,
    minimap: { enabled: false },
    wordWrap: "on",
    wrappingIndent: "indent",
    fontFamily: "'Fira Code', monospace",
    fontLigatures: true,
  });

  init().then(() => {
    const regex = compiler(editor.getValue());
    output?.setValue(regex);

    editor.getModel()?.onDidChangeContent(() => {
      try {
        const regex = compiler(editor.getValue());
        output?.setValue(regex);
      } catch (error) {
        output?.setValue("Parsing error");
      }
    });
  });
};

initEditors();
