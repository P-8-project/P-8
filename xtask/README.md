```
usage: cargo xtask <command>

A CLI allowing to run various tasks in the Viable repository

commands:
      run [arguments]          runs the viable_cli binary
      benchmark                runs benchmarks
      fuzz <crate>             runs fuzz testing on specific crates
      publish <target>         publishes specific crates or projects
      wasm <target>            builds wasm dependencies for specific projects


fuzz subcommands:
      compiler                 runs fuzz testing on the compiler


publish subcommands:
      cli                      publishes the viable_cli crate to crates.io
      compiler                 publishes the viable_compiler crate to crates.io
      playground               publishes the playground to vercel
      node                     publishes the viablec npm package
      extension <target>       publishes specific extensions


publish extension subcommands:
      vscode                   publishes the vscode extension to the market


wasm subcommands:
      playground               builds wasm dependencies for the playground
      node                     builds wasm dependencies for nodejs
```
