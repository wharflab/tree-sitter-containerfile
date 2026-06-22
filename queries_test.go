package tree_sitter_containerfile_test

import (
	"strings"
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

// TestShellCommandInjectionsAreNotCombined guards against a regression of
// issue #27. The bash injection for shell_command must NOT use
// (#set! injection.combined): combining merges every RUN body in the file into
// a single bash document, so the trailing token of one RUN fuses with the
// leading command word of the next (e.g. "migrations.sh" + "bundle"), and every
// command from the second RUN onward loses its highlighting. Each shell_command
// must instead inject as its own document.
func TestShellCommandInjectionsAreNotCombined(t *testing.T) {
	// Mirrors the reproduction from issue #27: two RUN instructions separated
	// by a COPY. The first RUN ends in "migrations.sh"; the second starts with
	// "bundle". With injection.combined they would parse as one bash document.
	source := []byte(`ARG IMAGE_TAG
FROM $IMAGE_TAG

RUN chmod +x \
    /entrypoints/kafka-consumer.sh \
    /entrypoints/migrations.sh

COPY . /app

RUN bundle install --jobs $(nproc)
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

	captureNames := query.CaptureNames()
	var bashRegions []string
	matches := cursor.Matches(query, tree.RootNode(), source)
	for match := matches.Next(); match != nil; match = matches.Next() {
		var language string
		combined := false
		for _, property := range query.PropertySettings(match.PatternIndex) {
			switch property.Key {
			case "injection.language":
				if property.Value != nil {
					language = *property.Value
				}
			case "injection.combined":
				combined = true
			}
		}
		if language != "bash" {
			continue
		}
		if combined {
			t.Errorf("bash injection must not set injection.combined (issue #27); "+
				"combining fuses adjacent RUN bodies into one document")
		}
		for _, capture := range match.Captures {
			if captureNames[capture.Index] == "injection.content" {
				bashRegions = append(bashRegions, capture.Node.Utf8Text(source))
			}
		}
	}

	// Each RUN body (shell_command) must be its own injected region. The
	// heredoc-based bash injection does not fire here, so exactly the two
	// shell_command bodies are expected.
	if len(bashRegions) != 2 {
		t.Fatalf("expected 2 separate bash injection regions, got %d: %#v", len(bashRegions), bashRegions)
	}

	// Sanity-check that the two RUN bodies remained distinct documents: the
	// second region must begin with the "bundle" command word rather than being
	// glued onto the first region's trailing "migrations.sh".
	if !strings.HasPrefix(strings.TrimSpace(bashRegions[1]), "bundle") {
		t.Errorf("second bash region should start with the \"bundle\" command, got %q", bashRegions[1])
	}
}
