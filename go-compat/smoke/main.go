package main

import (
	"fmt"
	"os"

	sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_containerfile "github.com/wharflab/tree-sitter-containerfile/bindings/go"
)

func fail(format string, args ...any) {
	fmt.Fprintf(os.Stderr, format+"\n", args...)
	os.Exit(1)
}

type parseCase struct {
	name   string
	source string
	checks []func(source []byte, root *sitter.Node) error
}

func main() {
	lang := sitter.NewLanguage(tree_sitter_containerfile.Language())
	if lang == nil || lang.Inner == nil {
		fail("Language() returned nil")
	}

	parser := sitter.NewParser()
	defer parser.Close()

	if err := parser.SetLanguage(lang); err != nil {
		fail("SetLanguage failed: %v", err)
	}

	cases := []parseCase{
		{
			name:   "basic instructions",
			source: "FROM alpine:3.20\nRUN echo ok\n",
			checks: []func([]byte, *sitter.Node) error{
				expectRootKind("source_file"),
				expectNoErrors(),
				expectNamedChildKind(0, "from_instruction"),
				expectNamedChildKind(1, "run_instruction"),
			},
		},
		{
			name:   "copy from stage",
			source: "FROM golang:1.24 AS builder\nCOPY --from=builder /src/app /usr/bin/app\n",
			checks: []func([]byte, *sitter.Node) error{
				expectRootKind("source_file"),
				expectNoErrors(),
				expectNodeText("image_alias", "builder"),
				expectNodeText("param", "--from=builder"),
			},
		},
		{
			name:   "env expansion",
			source: "ARG VERSION=1.0\nENV APP_VERSION=${VERSION}\n",
			checks: []func([]byte, *sitter.Node) error{
				expectRootKind("source_file"),
				expectNoErrors(),
				expectNodeText("variable", "VERSION"),
			},
		},
		{
			name:   "heredoc",
			source: "RUN <<EOF\nhello\nEOF\n",
			checks: []func([]byte, *sitter.Node) error{
				expectRootKind("source_file"),
				expectNoErrors(),
				expectNodeText("heredoc_marker", "<<EOF"),
				expectNodeText("heredoc_end", "EOF"),
			},
		},
	}

	for _, c := range cases {
		source := []byte(c.source)
		tree := parser.Parse(source, nil)
		if tree == nil {
			fail("[%s] Parse returned nil tree", c.name)
		}

		root := tree.RootNode()
		for _, check := range c.checks {
			if err := check(source, root); err != nil {
				tree.Close()
				fail("[%s] %v", c.name, err)
			}
		}
		tree.Close()
	}

	fmt.Println("go consumer compatibility: ok")
}

func expectRootKind(want string) func([]byte, *sitter.Node) error {
	return func(_ []byte, root *sitter.Node) error {
		if got := root.Kind(); got != want {
			return fmt.Errorf("root kind = %q, want %q", got, want)
		}
		return nil
	}
}

func expectNoErrors() func([]byte, *sitter.Node) error {
	return func(_ []byte, root *sitter.Node) error {
		if root.HasError() {
			return fmt.Errorf("root node has parse errors; sexp=%s", root.ToSexp())
		}
		return nil
	}
}

func expectNamedChildKind(index uint, want string) func([]byte, *sitter.Node) error {
	return func(_ []byte, root *sitter.Node) error {
		cursor := root.Walk()
		defer cursor.Close()
		children := root.NamedChildren(cursor)
		if uint(len(children)) <= index {
			return fmt.Errorf("named child count = %d, want >= %d", len(children), index+1)
		}
		if got := children[index].Kind(); got != want {
			return fmt.Errorf("named child[%d] kind = %q, want %q", index, got, want)
		}
		return nil
	}
}

func expectNodeText(kind, wantText string) func([]byte, *sitter.Node) error {
	return func(source []byte, root *sitter.Node) error {
		node := findFirstDescendant(root, kind)
		if node == nil {
			return fmt.Errorf("no %s node found in tree", kind)
		}
		gotText := node.Utf8Text(source)
		if gotText != wantText {
			return fmt.Errorf(
				"%s node text = %q (bytes [%d,%d)), want %q",
				kind, gotText, node.StartByte(), node.EndByte(), wantText,
			)
		}
		return nil
	}
}

func findFirstDescendant(node *sitter.Node, kind string) *sitter.Node {
	if node == nil {
		return nil
	}
	if node.Kind() == kind {
		return node
	}
	for i := range node.NamedChildCount() {
		if child := node.NamedChild(i); child != nil {
			if found := findFirstDescendant(child, kind); found != nil {
				return found
			}
		}
	}
	return nil
}
