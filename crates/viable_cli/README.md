<p align="center">
    <img alt="Viable Logo" height="250px" src="https://user-images.githubusercontent.com/14347895/159065887-51a2d948-ae6f-48c4-9dd2-1ee69e76b19f.png">
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