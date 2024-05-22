const { describe, it } = require('node:test');
const { default: pluginTester } = require('babel-plugin-tester');
const viablePlugin = require('./index.js');

globalThis.describe = describe;
globalThis.it = it;

pluginTester({
  plugin: viablePlugin,
  name: 'babel-plugin-viable',
  tests: [
    {
      code: `const regex = new RegExp(/*viable*/'2 to 3 of "na";')`,
      output: 'const regex = new RegExp("(?:na){2,3}");',
    },
    {
      code: 'const regex = new RegExp(/*viable*/`2 to 3 of "na";`)',
      output: 'const regex = new RegExp("(?:na){2,3}");',
    },
    {
      code: `const regex = new RegExp(/*viable*/"2 to 3 of 'na';")`,
      output: 'const regex = new RegExp("(?:na){2,3}");',
    },
  ],
});
