package tree_sitter_containerfile_test

import (
	"testing"

	sitter "github.com/tree-sitter/go-tree-sitter"
	containerfile "github.com/wharflab/tree-sitter-containerfile"
)

func TestHighlightsQueryCompiles(t *testing.T) {
	if containerfile.HighlightsQuery == "" {
		t.Fatal("HighlightsQuery is empty")
	}
	query, err := containerfile.GetHighlightsQuery()
	if err != nil {
		t.Fatalf("highlights query failed to compile: %v", err)
	}
	defer query.Close()

	names := query.CaptureNames()
	if len(names) == 0 {
		t.Fatal("highlights query has no captures")
	}

	// Verify keyword capture exists
	found := false
	for _, name := range names {
		if name == "keyword" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected keyword capture, got: %v", names)
	}
}

func TestInjectionsQueryCompiles(t *testing.T) {
	if containerfile.InjectionsQuery == "" {
		t.Fatal("InjectionsQuery is empty")
	}
	query, err := containerfile.GetInjectionsQuery()
	if err != nil {
		t.Fatalf("injections query failed to compile: %v", err)
	}
	defer query.Close()

	names := query.CaptureNames()
	if len(names) == 0 {
		t.Fatal("injections query has no captures")
	}

	found := false
	for _, name := range names {
		if name == "injection.content" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected injection.content capture, got: %v", names)
	}
}

func TestCopyHeredocInjectionsByDestinationExtension(t *testing.T) {
	source := []byte(`FROM scratch
COPY <<EOF /etc/config.json
{"ok": true}
EOF
COPY --chmod=644 <<EOF ./config.yaml
ok: true
EOF
COPY <<EOF ./config.yml
ok: yes
EOF
COPY <<EOF ./Cargo.toml
[package]
name = "demo"
EOF
COPY <<EOF ./feed.xml
<root />
EOF
COPY <<EOF ./notes.txt
plain text
EOF
`)

	language := containerfile.GetLanguage()
	parser := sitter.NewParser()
	defer parser.Close()
	if err := parser.SetLanguage(language); err != nil {
		t.Fatalf("set language: %v", err)
	}

	tree := parser.Parse(source, nil)
	defer tree.Close()
	if tree.RootNode().HasError() {
		t.Fatalf("source parsed with errors:\n%s", tree.RootNode().ToSexp())
	}

	query, err := containerfile.GetInjectionsQuery()
	if err != nil {
		t.Fatalf("injections query failed to compile: %v", err)
	}
	defer query.Close()

	cursor := sitter.NewQueryCursor()
	defer cursor.Close()

	type injection struct {
		filename string
		language string
		content  string
	}

	var injections []injection
	matches := cursor.Matches(query, tree.RootNode(), source)
	captureNames := query.CaptureNames()
	for match := matches.Next(); match != nil; match = matches.Next() {
		var item injection
		for _, property := range query.PropertySettings(match.PatternIndex) {
			if property.Key == "injection.language" && property.Value != nil {
				item.language = *property.Value
			}
		}
		if item.language == "" {
			continue
		}

		for _, capture := range match.Captures {
			switch captureNames[capture.Index] {
			case "injection.filename":
				item.filename = capture.Node.Utf8Text(source)
			case "injection.content":
				item.content = capture.Node.Utf8Text(source)
			}
		}
		injections = append(injections, item)
	}

	expected := map[string]string{
		"/etc/config.json": "json",
		"./config.yaml":    "yaml",
		"./config.yml":     "yaml",
		"./Cargo.toml":     "toml",
		"./feed.xml":       "xml",
	}

	if len(injections) != len(expected) {
		t.Fatalf("expected %d COPY heredoc injections, got %d: %#v", len(expected), len(injections), injections)
	}

	for _, injection := range injections {
		language, ok := expected[injection.filename]
		if !ok {
			t.Fatalf("unexpected injection for %q: %#v", injection.filename, injection)
		}
		if injection.language != language {
			t.Errorf("expected %s injection for %q, got %s", language, injection.filename, injection.language)
		}
		if injection.content == "" {
			t.Errorf("expected heredoc content capture for %q", injection.filename)
		}
		delete(expected, injection.filename)
	}

	for filename, language := range expected {
		t.Errorf("missing %s injection for %q", language, filename)
	}
}
