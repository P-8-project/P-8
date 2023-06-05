<p align="center">
    <img alt="Viable Logo" height="250px" src="https://user-images.githubusercontent.com/14347895/159069181-53bce5b3-a831-43f1-8c14-af6c6ed7b92b.svg">
</p>

<p align="center">
NodeJS bindings for the Viable language compiler
</p>

## Install

```sh
npm install viablec
```
or

```sh
yarn add viablec
```

## Usage

```js
const { compiler } = require("viablec");

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