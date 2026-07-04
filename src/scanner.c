#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "tree_sitter/parser.h"

enum {
    MAX_HEREDOCS = 10,
    DEL_SPACE = 512,
    SERIALIZED_HEADER_SIZE = 6,
    DIRECTIVE_KEY_CAPACITY = 16,
    CHECK_DIRECTIVE_LEN = 5,
    ESCAPE_DIRECTIVE_LEN = 6,
    SYNTAX_DIRECTIVE_LEN = 6,
};

typedef struct {
    bool in_heredoc;
    bool stripping_heredoc;
    bool directive_allowed;
    bool escape_seen;
    bool at_line_start;
    int32_t escape_char;
    unsigned heredoc_count;
    char *heredocs[MAX_HEREDOCS];
} scanner_state;

enum TokenType {
    COMMENT,
    LINE_CONTINUATION,
    REQUIRED_LINE_CONTINUATION,
    NEWLINE,
    INVALID_JSON_ARRAY_SHELL_COMMAND,
    HEREDOC_MARKER,
    HEREDOC_LINE,
    HEREDOC_END,
    HEREDOC_NL,
    LITERAL_DOLLAR,
    KEYWORD_TERMINATOR,
    ERROR_SENTINEL,
};

typedef struct {
    const char *key;
    unsigned key_len;
    int32_t value;
    unsigned value_len;
} parser_directive;

static char serialize_bool(bool value) {
    if (value) {
        return (char)1;
    }
    return (char)0;
}

static bool deserialize_bool(char value) { return value != '\0'; }

static char serialize_char(int32_t value) { return (char)(unsigned char)value; }

static int32_t deserialize_char(char value) {
    return (int32_t)(unsigned char)value;
}

void *tree_sitter_containerfile_external_scanner_create() {
    scanner_state *state = malloc(sizeof(scanner_state));
    if (!state) {
        return NULL;
    }
    memset(state, 0, sizeof(scanner_state));
    state->directive_allowed = true;
    state->at_line_start = true;
    state->escape_char = '\\';
    return state;
}

static void clear_heredocs(scanner_state *state) {
    for (unsigned i = 0; i < MAX_HEREDOCS; i++) {
        if (state->heredocs[i]) {
            free(state->heredocs[i]);
            state->heredocs[i] = NULL;
        }
    }

    state->heredoc_count = 0;
    state->in_heredoc = false;
    state->stripping_heredoc = false;
}

void tree_sitter_containerfile_external_scanner_destroy(void *payload) {
    if (!payload) {
        return;
    }

    scanner_state *state = payload;
    clear_heredocs(state);
    free(state);
}

unsigned tree_sitter_containerfile_external_scanner_serialize(void *payload,
                                                           char *buffer) {
    scanner_state *state = payload;

    unsigned pos = 0;
    buffer[pos++] = serialize_bool(state->in_heredoc);
    buffer[pos++] = serialize_bool(state->stripping_heredoc);
    buffer[pos++] = serialize_bool(state->directive_allowed);
    buffer[pos++] = serialize_bool(state->escape_seen);
    buffer[pos++] = serialize_bool(state->at_line_start);
    buffer[pos++] = serialize_char(state->escape_char);

    for (unsigned i = 0; i < state->heredoc_count; i++) {
        // Add the ending null byte to the length since we'll have to copy it as
        // well.
        unsigned len = strlen(state->heredocs[i]) + 1;

        // If we run out of space, just drop the heredocs that don't fit.
        // We need at least len + 1 bytes space since we'll copy len bytes below
        // and later add a null byte at the end.
        if (pos + len + 1 > TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
            break;
        }

        memcpy(&buffer[pos], state->heredocs[i], len);
        pos += len;
    }

    // Add a null byte at the end to make it easy to detect.
    buffer[pos++] = 0;
    return pos;
}

void tree_sitter_containerfile_external_scanner_deserialize(void *payload,
                                                         const char *buffer,
                                                         unsigned length) {
    scanner_state *state = payload;
    clear_heredocs(state);

    state->directive_allowed = true;
    state->escape_seen = false;
    state->at_line_start = true;
    state->escape_char = '\\';

    if (length < SERIALIZED_HEADER_SIZE) {
        return;
    }

    unsigned pos = 0;
    state->in_heredoc = deserialize_bool(buffer[pos++]);
    state->stripping_heredoc = deserialize_bool(buffer[pos++]);

    state->directive_allowed = deserialize_bool(buffer[pos++]);
    state->escape_seen = deserialize_bool(buffer[pos++]);
    state->at_line_start = deserialize_bool(buffer[pos++]);
    state->escape_char = deserialize_char(buffer[pos++]);
    if (state->escape_char != '\\' && state->escape_char != '`') {
        state->escape_char = '\\';
    }

    unsigned heredoc_count = 0;
    for (unsigned i = 0; i < MAX_HEREDOCS && pos < length; i++) {
        const char *end = memchr(&buffer[pos], '\0', length - pos);
        if (!end) {
            break;
        }

        unsigned len = end - &buffer[pos];

        // We found the ending null byte which means that we're done.
        if (len == 0) {
            break;
        }

        // Account for the ending null byte in strings (again).
        len++;
        char *heredoc = malloc(len);
        if (!heredoc) {
            break;
        }
        memcpy(heredoc, &buffer[pos], len);
        state->heredocs[i] = heredoc;
        heredoc_count++;

        pos += len;
    }

    state->heredoc_count = heredoc_count;
}

static bool is_inline_space(int32_t codepoint) {
    return (codepoint != '\0' && codepoint != '\n' && codepoint != '\r' &&
            iswspace(codepoint) != 0) != 0;
}

static bool is_directive_space(int32_t codepoint) {
    return (codepoint == ' ' || codepoint == '\t') != 0;
}

static bool is_ascii_alpha(int32_t codepoint) {
    return ((codepoint >= 'a' && codepoint <= 'z') ||
            (codepoint >= 'A' && codepoint <= 'Z')) != 0;
}

static int32_t to_lower_ascii(int32_t codepoint) {
    return codepoint >= 'A' && codepoint <= 'Z'
               ? codepoint - 'A' + 'a'
               : codepoint;
}

static bool is_line_end(TSLexer *lexer) {
    return (lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
            lexer->eof(lexer)) != 0;
}

static void skip_inline_whitespace(TSLexer *lexer) {
    while (is_inline_space(lexer->lookahead)) {
        lexer->advance(lexer, true);
    }
}

static void skip_leading_tabs(TSLexer *lexer) {
    while (lexer->lookahead == '\t') {
        lexer->advance(lexer, true);
    }
}

static void push_heredoc(scanner_state *state, char *delimiter) {
    if (state->heredoc_count == 0) {
        state->heredoc_count = 1;
        state->heredocs[0] = delimiter;
        state->stripping_heredoc = delimiter[0] == '-';
    } else if (state->heredoc_count >= MAX_HEREDOCS) {
        free(delimiter);
    } else {
        state->heredocs[state->heredoc_count++] = delimiter;
    }
}

static void pop_heredoc(scanner_state *state) {
    if (state->heredoc_count == 0) {
        return;
    }

    free(state->heredocs[0]);

    for (unsigned i = 1; i < state->heredoc_count; i++) {
        state->heredocs[i - 1] = state->heredocs[i];
    }
    state->heredocs[state->heredoc_count - 1] = NULL;
    state->heredoc_count--;

    if (state->heredoc_count > 0) {
        state->stripping_heredoc = state->heredocs[0][0] == '-';
    } else {
        state->in_heredoc = false;
        state->stripping_heredoc = false;
    }
}

static void close_directive_prologue(scanner_state *state) {
    state->directive_allowed = false;
}

static bool scan_newline(scanner_state *state, TSLexer *lexer, int symbol) {
    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
        }
    } else if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
    } else {
        return false;
    }

    if (state->directive_allowed) {
        close_directive_prologue(state);
    }

    state->at_line_start = true;
    if (symbol == HEREDOC_NL) {
        state->in_heredoc = true;
    }
    lexer->result_symbol = symbol;
    return true;
}

static bool scan_line_continuation(scanner_state *state, TSLexer *lexer,
                                   const bool *valid_symbols) {
    if (lexer->lookahead != state->escape_char &&
        !is_inline_space(lexer->lookahead)) {
        return false;
    }

    skip_inline_whitespace(lexer);

    if (lexer->lookahead != state->escape_char) {
        return false;
    }

    lexer->advance(lexer, false);

    if (valid_symbols[REQUIRED_LINE_CONTINUATION] &&
        (lexer->lookahead == '\n' || lexer->lookahead == '\r')) {
        close_directive_prologue(state);
        state->at_line_start = true;
        lexer->result_symbol = REQUIRED_LINE_CONTINUATION;

        if (lexer->lookahead == '\r') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == '\n') {
                lexer->advance(lexer, false);
            }
        } else {
            lexer->advance(lexer, false);
        }
        return true;
    }

    if (!valid_symbols[LINE_CONTINUATION]) {
        return false;
    }

    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }

    if (lexer->lookahead != '\n' && lexer->lookahead != '\r') {
        return false;
    }

    close_directive_prologue(state);
    state->at_line_start = true;
    lexer->result_symbol = LINE_CONTINUATION;

    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
        }
    } else {
        lexer->advance(lexer, false);
    }

    return true;
}

static bool consume_json_array_line_continuation(scanner_state *state,
                                                TSLexer *lexer) {
    if (lexer->lookahead != state->escape_char) {
        return false;
    }

    lexer->advance(lexer, false);

    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }

    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
        }
        return true;
    }

    if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        return true;
    }

    return false;
}

static void skip_json_array_whitespace(scanner_state *state, TSLexer *lexer) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }

    while (lexer->lookahead == state->escape_char &&
           consume_json_array_line_continuation(state, lexer)) {
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            lexer->advance(lexer, false);
        }
    }
}

static void consume_to_line_end(TSLexer *lexer) {
    while (!is_line_end(lexer)) {
        lexer->advance(lexer, false);
    }
}

static bool scan_json_escape_sequence(TSLexer *lexer) {
    switch (lexer->lookahead) {
    case '"':
    case '\\':
    case '/':
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't':
        lexer->advance(lexer, false);
        return true;

    case 'u':
        lexer->advance(lexer, false);
        for (unsigned i = 0; i < 4; i++) {
            if (!iswxdigit(lexer->lookahead)) {
                return false;
            }
            lexer->advance(lexer, false);
        }
        return true;

    default:
        return false;
    }
}

static bool scan_json_string(TSLexer *lexer) {
    if (lexer->lookahead != '"') {
        return false;
    }

    lexer->advance(lexer, false);

    for (;;) {
        if (is_line_end(lexer)) {
            return false;
        }

        switch (lexer->lookahead) {
        case '"':
            lexer->advance(lexer, false);
            return true;

        case '\\':
            lexer->advance(lexer, false);
            if (!scan_json_escape_sequence(lexer)) {
                return false;
            }
            break;

        default:
            lexer->advance(lexer, false);
            break;
        }
    }
}

static bool scan_invalid_json_array_shell_command(scanner_state *state,
                                                 TSLexer *lexer) {
    if (lexer->lookahead != '[') {
        return false;
    }

    lexer->advance(lexer, false);
    skip_json_array_whitespace(state, lexer);

    if (lexer->lookahead == ']') {
        lexer->advance(lexer, false);
        skip_json_array_whitespace(state, lexer);
        if (is_line_end(lexer)) {
            return false;
        }
        lexer->result_symbol = INVALID_JSON_ARRAY_SHELL_COMMAND;
        return true;
    }

    for (;;) {
        if (!scan_json_string(lexer)) {
            consume_to_line_end(lexer);
            lexer->result_symbol = INVALID_JSON_ARRAY_SHELL_COMMAND;
            return true;
        }

        skip_json_array_whitespace(state, lexer);

        if (lexer->lookahead == ',') {
            lexer->advance(lexer, false);
            skip_json_array_whitespace(state, lexer);
            continue;
        }

        if (lexer->lookahead == ']') {
            lexer->advance(lexer, false);
            skip_json_array_whitespace(state, lexer);
            if (is_line_end(lexer)) {
                return false;
            }
            consume_to_line_end(lexer);
            lexer->result_symbol = INVALID_JSON_ARRAY_SHELL_COMMAND;
            return true;
        }

        consume_to_line_end(lexer);
        lexer->result_symbol = INVALID_JSON_ARRAY_SHELL_COMMAND;
        return true;
    }
}

static bool is_parser_directive(scanner_state *state,
                                const parser_directive *directive) {
    if (directive->key_len == ESCAPE_DIRECTIVE_LEN &&
        memcmp(directive->key, "escape", ESCAPE_DIRECTIVE_LEN) == 0) {
        if (directive->value_len != 1 || state->escape_seen ||
            (directive->value != '\\' && directive->value != '`')) {
            return false;
        }
        state->escape_char = directive->value;
        state->escape_seen = true;
        return true;
    }

    if (directive->value_len == 0) {
        return false;
    }
    if (directive->key_len == SYNTAX_DIRECTIVE_LEN &&
        memcmp(directive->key, "syntax", SYNTAX_DIRECTIVE_LEN) == 0) {
        return true;
    }

    return (directive->key_len == CHECK_DIRECTIVE_LEN &&
            memcmp(directive->key, "check", CHECK_DIRECTIVE_LEN) == 0) != 0;
}

static bool parse_directive_header(TSLexer *lexer, parser_directive *directive,
                                   char *key, unsigned key_capacity) {
    unsigned key_len = 0;
    int32_t value = 0;
    unsigned value_len = 0;

    while (is_directive_space(lexer->lookahead)) {
        lexer->advance(lexer, false);
    }

    while (is_ascii_alpha(lexer->lookahead)) {
        if (key_len < key_capacity) {
            key[key_len++] = (char)to_lower_ascii(lexer->lookahead);
        }
        lexer->advance(lexer, false);
    }

    while (is_directive_space(lexer->lookahead)) {
        lexer->advance(lexer, false);
    }

    if (key_len < key_capacity && lexer->lookahead == '=') {
        lexer->advance(lexer, false);
        while (is_directive_space(lexer->lookahead)) {
            lexer->advance(lexer, false);
        }

        while (!is_line_end(lexer) &&
               !is_directive_space(lexer->lookahead)) {
            if (value_len == 0) {
                value = lexer->lookahead;
            }
            value_len++;
            lexer->advance(lexer, false);
        }
    }

    directive->key = key;
    directive->key_len = key_len;
    directive->value = value;
    directive->value_len = value_len;

    return key_len < key_capacity;
}

static bool consume_line_end(TSLexer *lexer) {
    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
        }
        return true;
    }

    if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        return true;
    }

    return false;
}

static bool scan_comment(scanner_state *state, TSLexer *lexer) {
    if (lexer->lookahead != '#') {
        return false;
    }

    bool may_be_directive = (state->directive_allowed &&
                             state->at_line_start &&
                             lexer->get_column(lexer) == 0) != 0;
    bool directive = false;
    char key[DIRECTIVE_KEY_CAPACITY];

    lexer->advance(lexer, false);

    if (may_be_directive) {
        parser_directive candidate;
        if (parse_directive_header(lexer, &candidate, key, sizeof(key))) {
            directive = is_parser_directive(state, &candidate);
        }
    }

    while (!is_line_end(lexer)) {
        lexer->advance(lexer, false);
    }

    if (!directive && state->directive_allowed) {
        close_directive_prologue(state);
    }

    state->at_line_start = consume_line_end(lexer);
    lexer->result_symbol = COMMENT;
    return true;
}

static bool scan_marker(scanner_state *state, TSLexer *lexer) {
    skip_inline_whitespace(lexer);

    if (lexer->lookahead != '<') {
        return false;
    }
    lexer->advance(lexer, false);

    if (lexer->lookahead != '<') {
        return false;
    }
    lexer->advance(lexer, false);

    close_directive_prologue(state);
    state->at_line_start = false;

    bool stripping = false;
    if (lexer->lookahead == '-') {
        stripping = true;
        lexer->advance(lexer, false);
    }

    int32_t quote = 0;
    if (lexer->lookahead == '"' || lexer->lookahead == '\'') {
        quote = lexer->lookahead;
        lexer->advance(lexer, false);
    }

    // Reserve a reasonable amount of space for the heredoc delimiter string.
    // Most heredocs (like EOF, EOT, EOS, FILE, etc.) are pretty short so we'll
    // usually only need a few bytes. We're also limited to less than 1024 bytes
    // by tree-sitter since our state has to fit in
    // TREE_SITTER_SERIALIZATION_BUFFER_SIZE.
    char delimiter[DEL_SPACE];

    // We start recording the actual string at position 1 since we store whether
    // it's a stripping heredoc in the first position (with either a dash or a
    // space).
    unsigned del_idx = 1;

    while (lexer->lookahead != '\0' &&
           (quote ? lexer->lookahead != quote : !iswspace(lexer->lookahead))) {
        if (lexer->lookahead == '\\') {
            lexer->advance(lexer, false);

            if (lexer->lookahead == '\0') {
                return false;
            }
        }

        if (del_idx > 0) {
            delimiter[del_idx++] = serialize_char(lexer->lookahead);
        }
        lexer->advance(lexer, false);

        // If we run out of space, stop recording the delimiter but keep
        // advancing the lexer so the failed external token leaves the parser
        // at a consistent error point. Reserve two bytes: one for the strip
        // indicator and one for the terminating null byte.
        if (del_idx >= DEL_SPACE - 2) {
            del_idx = 0;
        }
    }

    if (quote) {
        if (lexer->lookahead != quote) {
            return false;
        }
        lexer->advance(lexer, false);
    }

    if (del_idx <= 1) {
        return false;
    }

    delimiter[0] = ' ';
    if (stripping) {
        delimiter[0] = '-';
    }
    delimiter[del_idx] = '\0';

    // We copy the delimiter string to the heap here since we can't store our
    // stack-allocated string in our state (which is stored on the heap).
    char *del_copy = malloc(del_idx + 1);
    if (!del_copy) {
        return false;
    }
    memcpy(del_copy, delimiter, del_idx + 1);

    push_heredoc(state, del_copy);

    lexer->result_symbol = HEREDOC_MARKER;
    return true;
}

// Match a `$` that does NOT begin a valid ${...} or $identifier expansion, so
// it is literal text (e.g. cost$, $5, $$, a trailing bare $). This needs one
// character of lookahead past the `$`, which a grammar regex token cannot do.
// A `$` followed by an identifier char or `{` is left for the grammar's
// expansion rules.
static bool scan_literal_dollar(TSLexer *lexer) {
    if (lexer->lookahead != '$') {
        return false;
    }

    lexer->advance(lexer, false);

    int32_t next = lexer->lookahead;
    bool starts_expansion =
        (next == '{' || next == '_' || is_ascii_alpha(next)) != 0;
    if (starts_expansion) {
        return false;
    }

    lexer->result_symbol = LITERAL_DOLLAR;
    return true;
}

static bool scan_content(scanner_state *state, TSLexer *lexer,
                         const bool *valid_symbols) {
    if (state->heredoc_count == 0) {
        state->in_heredoc = false;
        return false;
    }

    state->in_heredoc = true;

    if (state->stripping_heredoc) {
        skip_leading_tabs(lexer);
    }

    if (valid_symbols[HEREDOC_END]) {
        unsigned delim_idx = 1;
        // Look for the current heredoc delimiter.
        while (state->heredocs[0][delim_idx] != '\0' &&
               lexer->lookahead != '\0' &&
               lexer->lookahead == state->heredocs[0][delim_idx]) {
            lexer->advance(lexer, false);
            delim_idx++;
        }

        // Check if the entire delimiter matched as a complete line.
        if (state->heredocs[0][delim_idx] == '\0' && is_line_end(lexer)) {
            lexer->result_symbol = HEREDOC_END;
            pop_heredoc(state);
            return true;
        }
    }

    if (!valid_symbols[HEREDOC_LINE]) {
        return false;
    }

    lexer->result_symbol = HEREDOC_LINE;

    for (;;) {
        switch (lexer->lookahead) {
        case '\0':
            if (lexer->eof(lexer)) {
                state->in_heredoc = false;
                return true;
            }
            lexer->advance(lexer, false);
            break;

        case '\n':
            return true;

        default:
            lexer->advance(lexer, false);
        }
    }
}

bool tree_sitter_containerfile_external_scanner_scan(void *payload, TSLexer *lexer,
                                                  const bool *valid_symbols) {
    scanner_state *state = payload;

    if (valid_symbols[ERROR_SENTINEL] && state->in_heredoc) {
        return scan_content(state, lexer, valid_symbols);
    }

    if (state->in_heredoc &&
        (valid_symbols[HEREDOC_LINE] || valid_symbols[HEREDOC_END])) {
        return scan_content(state, lexer, valid_symbols);
    }

    // A zero-width word boundary right after an instruction keyword: only
    // whitespace, a line end, or EOF may follow, so glued forms (FROMalpine)
    // fail to parse. Checked before scan_line_continuation, which skips
    // inline whitespace and would move the lexer off the boundary. When the
    // lookahead is the escape char, we fall through instead of failing so a
    // line continuation is consumed as an extra first and the boundary is
    // re-checked at the start of the continued line; this matches BuildKit's
    // join-then-tokenize semantics (`FROM\<nl> alpine` is valid, while
    // `FROM\<nl>alpine` joins to FROMalpine and is not). Suppressed during
    // error recovery (ERROR_SENTINEL valid): zero-width tokens there can
    // prevent the parser from making progress.
    if (valid_symbols[KEYWORD_TERMINATOR] && !valid_symbols[ERROR_SENTINEL] &&
        (is_inline_space(lexer->lookahead) || is_line_end(lexer))) {
        lexer->mark_end(lexer);
        lexer->result_symbol = KEYWORD_TERMINATOR;
        return true;
    }

    if (valid_symbols[REQUIRED_LINE_CONTINUATION] ||
        valid_symbols[LINE_CONTINUATION]) {
        if (scan_line_continuation(state, lexer, valid_symbols)) {
            return true;
        }
    }

    if (valid_symbols[LITERAL_DOLLAR] && scan_literal_dollar(lexer)) {
        return true;
    }

    if (valid_symbols[INVALID_JSON_ARRAY_SHELL_COMMAND] &&
        scan_invalid_json_array_shell_command(state, lexer)) {
        return true;
    }

    if (valid_symbols[COMMENT] && scan_comment(state, lexer)) {
        return true;
    }

    if (valid_symbols[HEREDOC_MARKER] && scan_marker(state, lexer)) {
        return true;
    }

    // HEREDOC_NL only matches a linebreak if there are open heredocs. This is
    // necessary to avoid a conflict in the grammar since a normal line break
    // could either be the start of a heredoc or the end of an instruction.
    if (valid_symbols[HEREDOC_NL]) {
        if (state->heredoc_count > 0) {
            return scan_newline(state, lexer, HEREDOC_NL);
        }
    }

    if (valid_symbols[NEWLINE] && scan_newline(state, lexer, NEWLINE)) {
        return true;
    }

    if (valid_symbols[ERROR_SENTINEL]) {
        return scan_marker(state, lexer);
    }

    return false;
}
