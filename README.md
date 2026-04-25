# tree-sitter-containerfile

A Containerfile and Dockerfile grammar for [tree-sitter](https://tree-sitter.github.io/).

This repository packages the grammar originally published as
[`camdencheek/tree-sitter-dockerfile`](https://github.com/camdencheek/tree-sitter-dockerfile)
under the `tree-sitter-containerfile` name, with generated bindings for Node.js,
Python, Rust, Go, Swift, and C.

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
import tree_sitter_containerfile "github.com/wharflab/tree-sitter-containerfile/bindings/go"
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

## License

Repository packaging is licensed under [Apache-2.0](LICENSE). The imported
grammar and fixtures from Camden Cheek's tree-sitter-dockerfile are licensed
under [MIT](LICENSE-MIT-CamdenCheek).
