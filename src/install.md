# Install

## Cargo

```sh
cargo install viable_cli
```

## From Source

```sh
git clone https://github.com/yoav-lavi/viable.git
cd viable
cargo install --path crates/viable_cli
```

## Binary

- macOS binaries (aarch64 and x86_64) can be downloaded from the [release page](https://github.com/yoav-lavi/viable/releases)

## Community

- [Brew](https://formulae.brew.sh/formula/viable) (macOS and Linux)
  <details><summary>Installation instructions</summary>

   ```sh
   brew install viable
   ```

  </details>

- [Arch Linux](https://aur.archlinux.org/packages/viable) (maintained by [@ilai-deutel](https://github.com/ilai-deutel))
  <details><summary>Installation instructions</summary>

  1. Installation with an AUR helper, for instance using `paru`:

     ```bash
     paru -Syu viable
     ```

  2. Install manually with `makepkg`:

     ```bash
     git clone https://aur.archlinux.org/viable.git
     cd viable
     makepkg -si
     ```

  </details>

- [NixOS](https://github.com/NixOS/nixpkgs/blob/master/pkgs/by-name/me/viable/package.nix) (maintained by [@jyooru](https://github.com/jyooru))
  <details><summary>Installation instructions</summary>

  Should be the following once the registry is updated.

  If you've successfuly installed via this method please open an issue and let me know.

  Thanks!

   ```sh
   nix-env -i viable
   ```

  </details>
