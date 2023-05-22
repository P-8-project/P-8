<p align="center">
    <img alt="Viable Logo" height="250px" src="https://user-images.githubusercontent.com/14347895/157926614-8434c590-e810-494c-ac9d-3657e9aa4583.png">
</p>

<p align="center">
A CLI wrapping the Viable language compiler
</p>

## Install

```sh
cargo install viable_cli
```

## Usage

```sh
viable [OPTIONS] [INPUT_FILE_PATH]

ARGS:
    <INPUT_FILE_PATH>    Read from a file

OPTIONS:
    -h, --help                         Print help information
    -n, --no-color                     Print output with no color
    -o, --output <OUTPUT_FILE_PATH>    Write to a file
    -r, --repl                         Start the Viable REPL
    -V, --version                      Print version information
```