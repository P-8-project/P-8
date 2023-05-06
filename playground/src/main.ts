import * as monaco from "monaco-editor";
import editorWorker from "monaco-editor/esm/vs/editor/editor.worker?worker";
// build from ../crates/viable_wasm included manually due to issues with wasm-bindgen (similar to https://github.com/rustwasm/wasm-bindgen/issues/113)
import init, { compiler } from "./wasm/viable_wasm";

declare global {
  interface Window {
    MonacoEnvironment: { getWorker: () => Worker };
  }
}

window.MonacoEnvironment = {
  getWorker: () => new editorWorker(),
};

const DEFAULT_EDITOR_SETTINGS: monaco.editor.IStandaloneEditorConstructionOptions =
  {
    theme: "vs-dark",
    automaticLayout: true,
    minimap: { enabled: false },
    wordWrap: "on",
    wrappingIndent: "indent",
    fontFamily: "'Fira Code', monospace",
    fontLigatures: true,
  };

const initEditors = async () => {
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
    ...DEFAULT_EDITOR_SETTINGS,
  });

  const output = monaco.editor.create(outputTarget, {
    value: ``,
    readOnly: true,
    ...DEFAULT_EDITOR_SETTINGS,
  });

  await init();

  const syncEditors = () => {
    try {
      const regex = compiler(editor.getValue());
      output.setValue(regex);
    } catch {
      output.setValue("Parsing error");
    }
  };

  syncEditors();

  editor.getModel()?.onDidChangeContent(syncEditors);
};

initEditors();
