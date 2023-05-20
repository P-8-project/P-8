import { editor, languages } from 'monaco-editor';
import editorWorker from 'monaco-editor/esm/vs/editor/editor.worker?worker';
import {
  DEFAULT_EDITOR_VALUE,
  MELODY_LANGUAGE_ID,
  NORD_THEME_ID,
} from './consts';
import { DEFAULT_EDITOR_SETTINGS } from './editor-settings';
import { languageDefinition } from './language-definition';
import { nordTheme } from './nord-theme';
// build from ../crates/viable_wasm included manually due to issues with wasm-bindgen (similar to https://github.com/rustwasm/wasm-bindgen/issues/113)
import init, { compiler } from '../wasm/viable_wasm';

window.MonacoEnvironment = {
  getWorker: () => new editorWorker(),
};

languages.register({ id: MELODY_LANGUAGE_ID });

languages.setMonarchTokensProvider(MELODY_LANGUAGE_ID, languageDefinition);

editor.defineTheme(NORD_THEME_ID, nordTheme);
editor.setTheme(NORD_THEME_ID);

const urlParams = new URLSearchParams(location.search);

const viableEditorTarget = document.getElementById('editor-container');
const regexEditorTarget = document.getElementById('output-container');

const getEditorInitialValue = () => {
  const urlParamContent = urlParams.get('content');
  if (urlParamContent !== null) {
    return decodeURIComponent(atob(urlParamContent));
  }
  return DEFAULT_EDITOR_VALUE;
};

const initialValue = getEditorInitialValue();

export const initEditors = async () => {
  if (!viableEditorTarget || !regexEditorTarget) {
    return;
  }

  const viableEditor = editor.create(viableEditorTarget, {
    value: initialValue,
    language: MELODY_LANGUAGE_ID,
    ...DEFAULT_EDITOR_SETTINGS,
  });

  const lineNumber = initialValue.split('\n').length + 1;

  viableEditor.setPosition({ lineNumber, column: 0 });
  viableEditor.focus();

  const regexEditor = editor.create(regexEditorTarget, {
    value: '',
    readOnly: true,
    ...DEFAULT_EDITOR_SETTINGS,
  });

  await init();

  const syncEditors = () => {
    try {
      window.currentEditorContent = viableEditor.getValue();
      const regex = compiler(viableEditor.getValue());
      regexEditor.setValue(regex);
    } catch (error) {
      regexEditor.setValue(error as string);
    }
  };

  syncEditors();

  viableEditor.getModel()?.onDidChangeContent(syncEditors);
};
