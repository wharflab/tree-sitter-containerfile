#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "tree_sitter/parser.h"

#define MAX_HEREDOCS 10
#define DEL_SPACE 512

typedef struct {
    bool in_heredoc;
    bool stripping_heredoc;
    bool directive_allowed;
    bool previous_parser_directive;
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
    HEREDOC_MARKER,
    HEREDOC_LINE,
    HEREDOC_END,
    HEREDOC_NL,
    ERROR_SENTINEL,
};

void *tree_sitter_containerfile_external_scanner_create() {
    scanner_state *state = malloc(sizeof(scanner_state));
    if (!state)
        return NULL;
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
    if (!payload)
        return;

    scanner_state *state = payload;
    clear_heredocs(state);
    free(state);
}

unsigned tree_sitter_containerfile_external_scanner_serialize(void *payload,
                                                           char *buffer) {
    scanner_state *state = payload;

    unsigned pos = 0;
    buffer[pos++] = state->in_heredoc;
    buffer[pos++] = state->stripping_heredoc;
    buffer[pos++] = state->directive_allowed;
    buffer[pos++] = state->previous_parser_directive;
    buffer[pos++] = state->escape_seen;
    buffer[pos++] = state->at_line_start;
    buffer[pos++] = (char)state->escape_char;

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
    state->previous_parser_directive = false;
    state->escape_seen = false;
    state->at_line_start = true;
    state->escape_char = '\\';

    if (length < 2) {
        return;
    } else {
        unsigned pos = 0;
        state->in_heredoc = buffer[pos++];
        state->stripping_heredoc = buffer[pos++];

        if (length >= 7) {
            state->directive_allowed = buffer[pos++];
            state->previous_parser_directive = buffer[pos++];
            state->escape_seen = buffer[pos++];
            state->at_line_start = buffer[pos++];
            state->escape_char = buffer[pos++];
            if (state->escape_char != '\\' && state->escape_char != '`') {
                state->escape_char = '\\';
            }
        }

        unsigned heredoc_count = 0;
        for (unsigned i = 0; i < MAX_HEREDOCS && pos < length; i++) {
            const char *end = memchr(&buffer[pos], '\0', length - pos);
            if (!end)
                break;

            unsigned len = end - &buffer[pos];

            // We found the ending null byte which means that we're done.
            if (len == 0)
                break;

            // Account for the ending null byte in strings (again).
            len++;
            char *heredoc = malloc(len);
            if (!heredoc)
                break;
            memcpy(heredoc, &buffer[pos], len);
            state->heredocs[i] = heredoc;
            heredoc_count++;

            pos += len;
        }

        state->heredoc_count = heredoc_count;
    }
}

static bool is_inline_space(int32_t c) {
    return c != '\0' && c != '\n' && c != '\r' && iswspace(c);
}

static bool is_directive_space(int32_t c) { return c == ' ' || c == '\t'; }

static int32_t to_lower_ascii(int32_t c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static bool is_line_end(TSLexer *lexer) {
    return lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
           lexer->eof(lexer);
}

static void skip_inline_whitespace(TSLexer *lexer) {
    while (is_inline_space(lexer->lookahead))
        lexer->advance(lexer, true);
}

static void skip_leading_tabs(TSLexer *lexer) {
    while (lexer->lookahead == '\t')
        lexer->advance(lexer, true);
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
    if (state->heredoc_count == 0)
        return;

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
    state->previous_parser_directive = false;
}

static bool scan_newline(scanner_state *state, TSLexer *lexer, int symbol) {
    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n')
            lexer->advance(lexer, false);
    } else if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
    } else {
        return false;
    }

    if (state->directive_allowed && !state->previous_parser_directive) {
        close_directive_prologue(state);
    } else {
        state->previous_parser_directive = false;
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
    skip_inline_whitespace(lexer);

    if (lexer->lookahead != state->escape_char)
        return false;

    lexer->advance(lexer, false);

    if (valid_symbols[REQUIRED_LINE_CONTINUATION] &&
        (lexer->lookahead == '\n' || lexer->lookahead == '\r')) {
        close_directive_prologue(state);
        state->at_line_start = true;
        lexer->result_symbol = REQUIRED_LINE_CONTINUATION;

        if (lexer->lookahead == '\r') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == '\n')
                lexer->advance(lexer, false);
        } else {
            lexer->advance(lexer, false);
        }
        return true;
    }

    if (!valid_symbols[LINE_CONTINUATION])
        return false;

    while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
        lexer->advance(lexer, false);

    if (lexer->lookahead != '\n' && lexer->lookahead != '\r')
        return false;

    close_directive_prologue(state);
    state->at_line_start = true;
    lexer->result_symbol = LINE_CONTINUATION;

    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n')
            lexer->advance(lexer, false);
    } else {
        lexer->advance(lexer, false);
    }

    return true;
}

static bool is_parser_directive(scanner_state *state, const char *key,
                                unsigned key_len, int32_t value,
                                bool valid_value) {
    if (key_len == 6 && memcmp(key, "escape", 6) == 0) {
        if (!valid_value || state->escape_seen ||
            (value != '\\' && value != '`')) {
            return false;
        }
        state->escape_char = value;
        state->escape_seen = true;
        return true;
    }

    return (key_len == 6 && memcmp(key, "syntax", 6) == 0) ||
           (key_len == 5 && memcmp(key, "check", 5) == 0);
}

static bool scan_comment(scanner_state *state, TSLexer *lexer) {
    if (lexer->lookahead != '#')
        return false;

    bool may_be_directive = state->directive_allowed &&
                            state->at_line_start &&
                            lexer->get_column(lexer) == 0;
    bool directive = false;
    char key[16];
    unsigned key_len = 0;
    int32_t value = 0;
    bool valid_value = false;

    lexer->advance(lexer, false);

    if (may_be_directive) {
        while (is_directive_space(lexer->lookahead))
            lexer->advance(lexer, false);

        while ((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
               (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z')) {
            if (key_len < sizeof(key)) {
                key[key_len++] = (char)to_lower_ascii(lexer->lookahead);
            }
            lexer->advance(lexer, false);
        }

        while (is_directive_space(lexer->lookahead))
            lexer->advance(lexer, false);

        if (key_len < sizeof(key) && lexer->lookahead == '=') {
            lexer->advance(lexer, false);
            while (is_directive_space(lexer->lookahead))
                lexer->advance(lexer, false);

            value = lexer->lookahead;
            if (!is_line_end(lexer)) {
                valid_value = true;
                lexer->advance(lexer, false);
            }
        }
    }

    while (!is_line_end(lexer)) {
        lexer->advance(lexer, false);
    }

    bool consumed_newline = false;
    if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n')
            lexer->advance(lexer, false);
        consumed_newline = true;
    } else if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        consumed_newline = true;
    }

    if (may_be_directive && key_len < sizeof(key)) {
        directive =
            is_parser_directive(state, key, key_len, value, valid_value);
    }

    if (directive) {
        state->previous_parser_directive = false;
    } else if (state->directive_allowed) {
        close_directive_prologue(state);
    }

    state->at_line_start = consumed_newline;
    lexer->result_symbol = COMMENT;
    return true;
}

static bool scan_marker(scanner_state *state, TSLexer *lexer) {
    skip_inline_whitespace(lexer);

    if (lexer->lookahead != '<')
        return false;
    lexer->advance(lexer, false);

    if (lexer->lookahead != '<')
        return false;
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
            delimiter[del_idx++] = lexer->lookahead;
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

    if (del_idx <= 1)
        return false;

    delimiter[0] = stripping ? '-' : ' ';
    delimiter[del_idx] = '\0';

    // We copy the delimiter string to the heap here since we can't store our
    // stack-allocated string in our state (which is stored on the heap).
    char *del_copy = malloc(del_idx + 1);
    if (!del_copy)
        return false;
    memcpy(del_copy, delimiter, del_idx + 1);

    push_heredoc(state, del_copy);

    lexer->result_symbol = HEREDOC_MARKER;
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

    if (!valid_symbols[HEREDOC_LINE])
        return false;

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

    if (valid_symbols[REQUIRED_LINE_CONTINUATION] ||
        valid_symbols[LINE_CONTINUATION]) {
        if (scan_line_continuation(state, lexer, valid_symbols))
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
