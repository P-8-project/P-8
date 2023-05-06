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
    theme: "nord",
    automaticLayout: true,
    minimap: { enabled: false },
    wordWrap: "on",
    wrappingIndent: "indent",
    fontFamily: "'Fira Code', monospace",
    fontLigatures: true,
    renderLineHighlight: "none",
    scrollbar: {
      alwaysConsumeMouseWheel: false,
      vertical: "hidden",
      horizontal: "hidden",
    },
  };

// colors taken from the Nord palette (https://www.nordtheme.com)
const nordTheme: monaco.editor.IStandaloneThemeData = {
  base: "vs-dark",
  inherit: false,
  rules: [
    { token: "keyword", foreground: "81A1C1" },
    { token: "digit", foreground: "EBCB8B" },
    { token: "string", foreground: "A3BE8C" },
    { token: "character", foreground: "EBCB8B" },
  ],
  colors: {
    "editor.background": "#2E3440",
    "editor.foreground": "#D8DEE9",
  },
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

  monaco.languages.register({
    id: "viable",
  });

  monaco.languages.setMonarchTokensProvider("viable", {
    tokenizer: {
      root: [
        [/(of|capture|to|of|some|match|over)/, "keyword"],
        [/[0-9]/, "digit"],
        [/"(\\"|[^"\n])*"/, "string"],
        [/'(\\'|[^'\n])*'/, "string"],
        [
          /(<space>|<newline>|<tab>|<return>|<feed>|<null>|<digit>|<word>|<vertical>)/,
          "character",
        ],
        [/(start|end|char)/, "character"],
        [/(A|Z|a|z)/, "character"],
      ],
    },
  });

  monaco.editor.defineTheme("nord", nordTheme);
  monaco.editor.setTheme("nord");

  const editor = monaco.editor.create(editorTarget, {
    value: initialValue,
    language: "viable",
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
