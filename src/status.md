# Future Feature Status

🐣 - Partially implemented

❌ - Not implemented

❔ - Unclear what the syntax will be

❓ - Unclear whether this will be implemented

| Viable                              | Regex                 | Status      |
| ----------------------------------- | --------------------- | ----------- |
| `not "A";`                          | `[^A]`                | 🐣          |
| variables / macros                  |                       | 🐣          |
| `<...::...>`                        | `\p{...}`             | 🐣          |
| `not <...::...>`                    | `\P{...}`             | 🐣          |
| file watcher                        |                       | ❌          |
| multiline groups in REPL            |                       | ❌          |
| `flags: global, multiline, ...`     | `/.../gm...`          | ❔          |
| (?)                                 | `\#`                  | ❔          |
| (?)                                 | `\k<name>`            | ❔          |
| (?)                                 | `\uYYYY`              | ❔          |
| (?)                                 | `\xYY`                | ❔          |
| (?)                                 | `\ddd`                | ❔          |
| (?)                                 | `\cY`                 | ❔          |
| (?)                                 | `$1`                  | ❔          |
| (?)                                 | <code>$\`</code>      | ❔          |
| (?)                                 | `$&`                  | ❔          |
| (?)                                 | `x20`                 | ❔          |
| (?)                                 | `x{06fa}`             | ❔          |
| `any of "a", "b", "c"` *            | `[abc]`               | ❓          |
| multiple ranges *                   | `[a-zA-Z0-9]`         | ❓          |
| regex optimization                  |                       | ❓          |
| standard library / patterns         |                       | ❓          |
| reverse compiler                    |                       | ❓          |

\* these are expressable in the current syntax using other methods