# Viable

<p align="center">
Deno bindings for the Viable language compiler
</p>

## Usage

```ts
import init, { compiler } from "https://deno.land/x/viable/viable_wasm.js";

await init();

const source = `
  <start>;

  option of "v";

  capture major {
    some of <digit>;
  }

  ".";

  capture minor {
    some of <digit>;
  }

  ".";

  capture patch {
    some of <digit>;
  }

  <end>;
`;

try {
  const output = compiler(source);
  new RegExp(output).test("v1.1.1"); // true
} catch (error) {
  // handle compilation error
}
```

## Links

- [Language Documentation](https://yoav-lavi.github.io/viable/book/)
- [Repository](https://github.com/yoav-lavi/viable)