# tree-sitter-containerfile

A maintained Containerfile and Dockerfile grammar for [tree-sitter](https://tree-sitter.github.io/).

This project is intended to be a reliable, actively maintained grammar for
modern container build files, with published packages and generated bindings
for Node.js, Python, Rust, Go, Swift, and C.

## Why This Package

Containerfile and Dockerfile syntax continues to evolve across Docker,
BuildKit, Podman, and related tooling. This grammar aims to close the gap for
modern Dockerfile features, keep real-world fixtures parsing cleanly, and ship
usable packages across the common tree-sitter binding ecosystems.

The historical `tree-sitter-dockerfile` npm package is not a dependable
installation target: it currently resolves to a `0.0.1-security` placeholder
release. `tree-sitter-containerfile` is the maintained package name for this
grammar and its generated artifacts.

## Installation

### npm

```sh
npm install tree-sitter-containerfile
```

### PyPI

```sh
pip install tree-sitter-containerfile
```

### Cargo

```sh
cargo add tree-sitter-containerfile
```

### Go

```go
import containerfile "github.com/wharflab/tree-sitter-containerfile"
```

## Usage

### Node.js

```js
import Parser from "tree-sitter";
import Containerfile from "tree-sitter-containerfile";

const parser = new Parser();
parser.setLanguage(Containerfile);

const tree = parser.parse("FROM alpine:3.20\nRUN echo ok\n");
console.log(tree.rootNode.toString());
```

### Python

```python
from tree_sitter import Language, Parser
import tree_sitter_containerfile

parser = Parser(Language(tree_sitter_containerfile.language()))
tree = parser.parse(b"FROM alpine:3.20\nRUN echo ok\n")
print(tree.root_node.sexp())
```

### Rust

```rust
let mut parser = tree_sitter::Parser::new();
let language = tree_sitter_containerfile::LANGUAGE;
parser.set_language(&language.into()).unwrap();

let tree = parser.parse("FROM alpine:3.20\nRUN echo ok\n", None).unwrap();
println!("{}", tree.root_node().to_sexp());
```

### Go

```go
package main

import (
	"fmt"

	sitter "github.com/tree-sitter/go-tree-sitter"
	containerfile "github.com/wharflab/tree-sitter-containerfile"
)

func main() {
	parser := sitter.NewParser()
	defer parser.Close()

	_ = parser.SetLanguage(containerfile.GetLanguage())
	tree := parser.Parse([]byte("FROM alpine:3.20\nRUN echo ok\n"), nil)
	defer tree.Close()

	fmt.Println(tree.RootNode().ToSexp())
}
```

## Development

```sh
npm ci
npm run generate
tree-sitter test
```

The test suite includes the upstream corpus and integration parsing of the
real-world Containerfile fixtures in `examples/`.

## Credits

This grammar started from
[`camdencheek/tree-sitter-dockerfile`](https://github.com/camdencheek/tree-sitter-dockerfile).
The project is being maintained and extended here under the
`tree-sitter-containerfile` package name.

## License

Licensed under [MIT](LICENSE).

The original MIT notice for the upstream grammar and fixtures is preserved in
[LICENSE-MIT-CamdenCheek](LICENSE-MIT-CamdenCheek).
