/// <reference types="tree-sitter-cli/dsl"/>
// @ts-check

/**
 * @param {RuleOrLiteral} atom
 * @param {RuleOrLiteral} continuation
 * @returns {RuleOrLiteral}
 */
const spacedValue = (atom, continuation) =>
  seq(repeat1(atom), repeat(continuation));

/**
 * @param {RuleOrLiteral} continuation
 * @returns {RuleOrLiteral}
 */
const continuedSpacedValue = (continuation) =>
  seq(continuation, repeat(continuation));

/**
 * @param {GrammarSymbols<string>} $
 * @param {RuleOrLiteral} atom
 * @returns {RuleOrLiteral}
 */
const spacedValueContinuation = ($, atom) =>
  seq(
    alias($.required_line_continuation, $.line_continuation),
    repeat1(atom),
  );

export default grammar({
  name: 'containerfile',

  extras: ($) => [/[ \t\r\f]+/, $.line_continuation, $.comment],
  externals: ($) => [
    $.comment,
    $.line_continuation,
    $.required_line_continuation,
    $._newline,
    $._invalid_json_array_shell_command,
    $.heredoc_marker,
    $._heredoc_line,
    $.heredoc_end,
    $._heredoc_nl,
    $.error_sentinel,
  ],

  rules: {
    source_file: ($) =>
      seq(
        repeat($._newline),
        optional(
          seq(
            $._instruction,
            repeat(seq(repeat1($._newline), $._instruction)),
            repeat($._newline),
          ),
        ),
      ),

    _instruction: ($) =>
      choice(
        $.from_instruction,
        $.run_instruction,
        $.cmd_instruction,
        $.label_instruction,
        $.expose_instruction,
        $.env_instruction,
        $.add_instruction,
        $.copy_instruction,
        $.entrypoint_instruction,
        $.volume_instruction,
        $.user_instruction,
        $.workdir_instruction,
        $.arg_instruction,
        $.onbuild_instruction,
        $.stopsignal_instruction,
        $.healthcheck_instruction,
        $.shell_instruction,
        $.maintainer_instruction,
        $.cross_build_instruction,
      ),

    from_instruction: ($) =>
      seq(
        alias(/[fF][rR][oO][mM]/, 'FROM'),
        optional($.param),
        $.image_spec,
        optional(seq(alias(/[aA][sS]/, 'AS'), field('as', $.image_alias))),
      ),

    run_instruction: ($) =>
      seq(
        alias(/[rR][uU][nN]/, 'RUN'),
        repeat(
          choice(
            $.param,
            $.mount_param,
          ),
        ),
        $._json_or_shell_command,
        repeat($.heredoc_block),
      ),

    cmd_instruction: ($) =>
      seq(
        alias(/[cC][mM][dD]/, 'CMD'),
        $._json_or_shell_command,
      ),

    label_instruction: ($) =>
      seq(
        alias(/[lL][aA][bB][eE][lL]/, 'LABEL'),
        choice(repeat1($.label_pair), alias($._spaced_label_pair, $.label_pair)),
      ),

    expose_instruction: ($) =>
      seq(
        alias(/[eE][xX][pP][oO][sS][eE]/, 'EXPOSE'),
        repeat1($.expose_port),
      ),

    env_instruction: ($) =>
      seq(
        alias(/[eE][nN][vV]/, 'ENV'),
        choice(repeat1($.env_pair), alias($._spaced_env_pair, $.env_pair)),
      ),

    add_instruction: ($) =>
      seq(
        alias(/[aA][dD][dD]/, 'ADD'),
        repeat($.param),
        choice(
          $.json_string_array,
          seq(
            repeat1(
              seq(alias($.path_with_heredoc, $.path), $._non_newline_whitespace),
            ),
            alias($.path_with_heredoc, $.path),
            repeat($.heredoc_block),
          ),
        ),
      ),

    copy_instruction: ($) =>
      seq(
        alias(/[cC][oO][pP][yY]/, 'COPY'),
        repeat($.param),
        choice(
          $.json_string_array,
          seq(
            repeat1(
              seq(alias($.path_with_heredoc, $.path), $._non_newline_whitespace),
            ),
            alias($.path_with_heredoc, $.path),
            repeat($.heredoc_block),
          ),
        ),
      ),

    entrypoint_instruction: ($) =>
      seq(
        alias(/[eE][nN][tT][rR][yY][pP][oO][iI][nN][tT]/, 'ENTRYPOINT'),
        $._json_or_shell_command,
      ),

    volume_instruction: ($) =>
      seq(
        alias(/[vV][oO][lL][uU][mM][eE]/, 'VOLUME'),
        choice(
          $.json_string_array,
          seq($.path, repeat(seq($._non_newline_whitespace, $.path))),
        ),
      ),

    user_instruction: ($) =>
      seq(
        alias(/[uU][sS][eE][rR]/, 'USER'),
        field('user', alias($._user_name_or_group, $.unquoted_string)),
        optional(
          seq(
            token.immediate(':'),
            field('group',
              alias($._immediate_user_name_or_group, $.unquoted_string)),
          ),
        ),
      ),

    _user_name_or_group: ($) =>
      seq(
        choice(/([a-zA-Z_][-A-Za-z0-9_.]*|[0-9]+)/, $.expansion),
        repeat($._immediate_user_name_or_group_fragment),
      ),

    // same as _user_name_or_group but sticks to previous token
    _immediate_user_name_or_group: ($) =>
      repeat1($._immediate_user_name_or_group_fragment),

    _immediate_user_name_or_group_fragment: ($) =>
      choice(
        token.immediate(/([a-zA-Z_][-a-zA-Z0-9_.]*|[0-9]+)/),
        $._immediate_expansion,
      ),

    workdir_instruction: ($) =>
      seq(alias(/[wW][oO][rR][kK][dD][iI][rR]/, 'WORKDIR'), $.path),

    arg_instruction: ($) =>
      seq(
        alias(/[aA][rR][gG]/, 'ARG'),
        repeat1(
          seq(
            $._non_newline_whitespace,
            $.arg_pair,
          ),
        ),
      ),

    arg_pair: ($) =>
      seq(
        field('name', alias(/[a-zA-Z0-9_]+/, $.unquoted_string)),
        optional(
          seq(
            token.immediate('='),
            field('default',
              choice(
                $.double_quoted_string,
                $.single_quoted_string,
                $.unquoted_string,
              )),
          ),
        ),
      ),

    onbuild_instruction: ($) =>
      seq(alias(/[oO][nN][bB][uU][iI][lL][dD]/, 'ONBUILD'), $._instruction),

    stopsignal_instruction: ($) =>
      seq(
        alias(/[sS][tT][oO][pP][sS][iI][gG][nN][aA][lL]/, 'STOPSIGNAL'),
        $._stopsignal_value,
      ),

    _stopsignal_value: ($) =>
      choice(
        $.double_quoted_string,
        seq(
          choice(/[A-Z0-9+-]+/, $.expansion),
          repeat(choice(token.immediate(/[A-Z0-9+-]+/), $._immediate_expansion)),
        ),
      ),

    healthcheck_instruction: ($) =>
      seq(
        alias(/[hH][eE][aA][lL][tT][hH][cC][hH][eE][cC][kK]/, 'HEALTHCHECK'),
        optional(
          choice(
            'NONE',
            seq(
              repeat($.param),
              choice(
                alias($._healthcheck_cmd_instruction, $.cmd_instruction),
                alias($._healthcheck_raw_command, $.shell_command),
              ),
            ),
          ),
        ),
      ),

    _healthcheck_cmd_instruction: ($) =>
      seq(
        alias(/[cC][mM][dD]/, 'CMD'),
        optional(choice($.json_string_array, $.shell_command)),
      ),

    _healthcheck_raw_command: ($) =>
      seq(
        $._healthcheck_raw_fragment,
        repeat(
          seq(
            alias($.required_line_continuation, $.line_continuation),
            repeat($._newline),
            $._shell_fragment,
          ),
        ),
      ),

    _healthcheck_raw_fragment: ($) =>
      seq(
        $._healthcheck_raw_fragment_atom,
        repeat($._shell_fragment_atom),
      ),

    _healthcheck_raw_fragment_atom: ($) =>
      choice(
        seq($.heredoc_marker, /[ \t]*/),
        /"([^"\\`\n]|[\\`].)*"/,
        /'([^'\\`\n]|[\\`].)*'/,
        $._shell_double_quoted_fragment,
        $._shell_single_quoted_fragment,
        /[,=-]/,
        /[^cC\\`\[\n#\s,=\-"']([^\\`\n<"']|[\\`][^ \t\n\r])*/,
        /[cC]([^mM\\`\n<"']|[\\`][^ \t\n\r])([^\\`\n<"']|[\\`][^ \t\n\r])*/,
        /[cC][mM]([^dD\\`\n<"']|[\\`][^ \t\n\r])([^\\`\n<"']|[\\`][^ \t\n\r])*/,
        /[cC][mM][dD][^\s\\`\n<"']([^\\`\n<"']|[\\`][^ \t\n\r])*/,
        /[cC][mM]?/,
        /[\\`][^\r\n,=-]/,
        /[\\`]/,
        /<[^<]/,
      ),

    shell_instruction: ($) =>
      seq(alias(/[sS][hH][eE][lL][lL]/, 'SHELL'), $.json_string_array),

    maintainer_instruction: () =>
      seq(
        alias(/[mM][aA][iI][nN][tT][aA][iI][nN][eE][rR]/, 'MAINTAINER'),
        /.*/,
      ),

    cross_build_instruction: () =>
      seq(
        alias(
          /[cC][rR][oO][sS][sS]_[bB][uU][iI][lL][dD][a-zA-Z_]*/,
          'CROSS_BUILD',
        ),
        /.*/,
      ),

    heredoc_block: ($) =>
      seq(
        // A heredoc block starts with a line break after the instruction it
        // belongs to. The herdoc_nl token is a special token that only matches
        // \n if there's at least one open heredoc to avoid conflicts.
        // We also alias this token to hide it from the output like all other
        // whitespace.
        $._heredoc_nl,
        repeat(seq($._heredoc_line, '\n')),
        $.heredoc_end,
      ),

    path: ($) =>
      seq(
        choice(
          /[^-\s\$<]/, // cannot start with a '-' to avoid conflicts with params
          /<[^<]/, // cannot start with a '<<' to avoid conflicts with heredocs (a single < is fine, though)
          $.expansion,
        ),
        repeat(choice(token.immediate(/[^\s\$]+/), $._immediate_expansion)),
      ),

    path_with_heredoc: ($) =>
      choice(
        $.heredoc_marker,
        seq(
          choice(
            /[^-\s\$<]/, // cannot start with a '-' to avoid conflicts with params
            /<[^-\s\$<]/,
            $.expansion,
          ),
          repeat(choice(token.immediate(/[^\s\$]+/), $._immediate_expansion)),
        ),
      ),

    expansion: $ =>
      seq('$', $._expansion_body),

    // we have 2 rules b/c aliases don't work as expected on seq() directly
    _immediate_expansion: $ => alias($._imm_expansion, $.expansion),
    _imm_expansion: $ =>
      seq(token.immediate('$'), $._expansion_body),

    _expansion_body: $ =>
      choice(
        $.variable,
        seq(
          token.immediate('{'),
          alias(token.immediate(/[^\}]+/), $.variable),
          token.immediate('}'),
        ),
      ),

    variable: () => token.immediate(/[a-zA-Z_][a-zA-Z0-9_]*/),

    env_pair: ($) =>
      seq(
        field('name', $._env_key),
        token.immediate('='),
        optional(
          field('value', $._env_assignment_value),
        ),
      ),

    _env_assignment_value: ($) =>
      choice(
        $.double_quoted_string,
        $._env_single_quoted_string_with_trailing_quote,
        $.unquoted_string,
      ),

    _env_single_quoted_string_with_trailing_quote: ($) =>
      seq($.single_quoted_string, optional($._env_trailing_single_quotes)),

    _env_trailing_single_quotes: () => token.immediate(/'+/),

    _spaced_env_pair: ($) =>
      seq(
        field('name', $._env_key),
        choice(
          seq(
            token.immediate(/\s+/),
            field('value',
              choice(
                $.double_quoted_string,
                $.single_quoted_string,
                alias($._spaced_env_value, $.unquoted_string),
              )),
          ),
          field('value', alias($._continued_spaced_env_value, $.unquoted_string)),
        ),
      ),

    _env_key: ($) =>
      alias(/[a-zA-Z_][a-zA-Z0-9_]*/, $.unquoted_string),

    expose_port: ($) =>
      seq(
        choice(/\d+(-\d+)?/, $.expansion),
        // Protocol is optional and case-insensitive; Docker normalizes it and
        // accepts tcp, udp, and sctp. It must abut the port (no space).
        optional(token.immediate(/\/[a-zA-Z]+/)),
      ),

    label_pair: ($) =>
      seq(
        field('key', $._label_key),
        token.immediate('='),
        field('value',
          choice(
            $.double_quoted_string,
            $.single_quoted_string,
            $.unquoted_string,
          )),
      ),

    _spaced_label_pair: ($) =>
      seq(
        field('key', $._label_key),
        // Legacy LABEL treats the first word as the key and the rest of the
        // line as the value, so quotes remain literal value text.
        choice(
          seq(
            token.immediate(/\s+/),
            field('value', alias($._spaced_label_value, $.unquoted_string)),
          ),
          field('value', alias($._continued_spaced_label_value, $.unquoted_string)),
        ),
      ),

    _label_key: ($) =>
      choice(
        alias(/[-a-zA-Z0-9\._]+/, $.unquoted_string),
        $.double_quoted_string,
        $.single_quoted_string,
      ),

    image_spec: ($) =>
      seq(
        field('name', $.image_name),
        field('tag', optional($.image_tag)),
        field('digest', optional($.image_digest)),
      ),

    image_name: ($) =>
      seq(
        choice(/[^@:\s\$-]/, $.expansion),
        repeat(choice(token.immediate(/[^@:\s\$]+/), $._immediate_expansion)),
      ),

    image_tag: ($) =>
      seq(
        token.immediate(':'),
        repeat1(choice(token.immediate(/[^@\s\$]+/), $._immediate_expansion)),
      ),

    image_digest: ($) =>
      seq(
        token.immediate('@'),
        repeat1(choice(token.immediate(/[a-zA-Z0-9:]+/), $._immediate_expansion)),
      ),

    // Generic parsing of options passed right after an instruction name.
    param: () =>
      seq(
        '--',
        field('name', token.immediate(/[a-z][-a-z]*/)),
        optional(
          seq(
            token.immediate('='),
            field('value', token.immediate(/[^\s]+/)),
          ),
        ),
      ),

    // Specific parsing of the --mount option e.g.
    //
    //   --mount=type=cache,target=/root/.cache/go-build
    //
    mount_param: ($) => seq(
      '--',
      field('name', token.immediate('mount')),
      token.immediate('='),
      field(
        'value',
        seq(
          $.mount_param_param,
          repeat(
            seq(token.immediate(','), $.mount_param_param),
          ),
        ),
      ),
    ),

    mount_param_param: () => seq(
      token.immediate(/[^\s=,]+/),
      optional(seq(
        token.immediate('='),
        token.immediate(/[^\s=,]+/),
      )),
    ),

    image_alias: ($) => seq(
      choice(/[-a-zA-Z0-9_.]+/, $.expansion),
      repeat(choice(token.immediate(/[-a-zA-Z0-9_.]+/), $._immediate_expansion)),
    ),

    _json_or_shell_command: ($) =>
      choice(
        alias($._invalid_json_array_shell_command, $.shell_command),
        prec(1, $.json_string_array),
        $.shell_command,
      ),

    shell_command: ($) =>
      seq(
        $._shell_fragment,
        repeat(
          seq(
            alias($.required_line_continuation, $.line_continuation),
            repeat($._newline),
            $._shell_fragment,
          ),
        ),
      ),

    _shell_fragment: ($) => repeat1(
      $._shell_fragment_atom,
    ),

    _shell_fragment_atom: ($) =>
      choice(
        // A shell fragment is broken into the same tokens as other
        // constructs because the lexer prefers the longer tokens
        // when it has a choice. The example below shows the tokenization
        // of the --mount parameter.
        //
        //   RUN --mount=foo=bar,baz=42 ls --all
        //       ^^     ^   ^   ^   ^
        //         ^^^^^ ^^^ ^^^ ^^^ ^^
        //       |--------param-------|
        //                              |--shell_command--|
        //
        seq($.heredoc_marker, /[ \t]*/),
        /"([^"\\`\n]|[\\`].)*"/,
        /'([^'\\`\n]|[\\`].)*'/,
        $._shell_double_quoted_fragment,
        $._shell_single_quoted_fragment,
        /[,=-]/,
        /[^\\`\[\n#\s,=\-"']([^\\`\n<"']|[\\`][^ \t\n\r])*/,
        /[\\`][^\r\n,=-]/,
        /[\\`]/,
        /<[^<]/,
      ),

    _shell_double_quoted_fragment: ($) =>
      seq(
        '"',
        repeat(
          choice(
            token.immediate(prec(1, /[^"\\`\n]+/)),
            token.immediate(/[\\`][^\r\n]/),
            alias($.required_line_continuation, $.line_continuation),
            $.line_continuation,
          ),
        ),
        token.immediate('"'),
      ),

    _shell_single_quoted_fragment: ($) =>
      seq(
        '\'',
        repeat(
          choice(
            token.immediate(prec(1, /[^'\\`\n]+/)),
            token.immediate(/[\\`][^\r\n]/),
            alias($.required_line_continuation, $.line_continuation),
            $.line_continuation,
          ),
        ),
        token.immediate('\''),
      ),

    json_string_array: ($) =>
      seq(
        '[',
        optional(
          seq($.json_string, repeat(seq(',', $.json_string))),
        ),
        ']',
      ),

    // Note that JSON strings are different from the other double-quoted
    // strings. They don't support $-expansions.
    // Convenient reference: https://www.json.org/
    json_string: ($) => seq(
      '"',
      repeat(
        choice(
          token.immediate(/[^"\\`\n]+/),
          alias($.json_escape_sequence, $.escape_sequence),
          token.immediate('`'),
        ),
      ),
      '"',
    ),

    json_escape_sequence: () => token.immediate(
      /\\(?:["\\/bfnrt]|u[0-9A-Fa-f]{4})/,
    ),

    double_quoted_string: ($) =>
      seq(
        '"',
        repeat(
          choice(
            token.immediate(/[^"\n\\`\$]+/),
            alias($.double_quoted_escape_sequence, $.escape_sequence),
            token.immediate(/\$\(/),
            '\\',
            '`',
            $._immediate_expansion,
          ),
        ),
        '"',
      ),

    // Single-quoted strings are fully literal in Dockerfile shell parsing:
    // there is no escape processing (a backslash or backtick before the
    // closing quote does NOT escape it) and no $-expansions. A backslash/
    // backtick before a newline is still a line continuation (BuildKit joins
    // physical lines before quote processing), which is handled by the
    // external line_continuation extra; an isolated backslash/backtick is
    // literal text.
    single_quoted_string: () =>
      seq(
        '\'',
        repeat(
          choice(
            token.immediate(/[^'\n\\`]+/),
            token.immediate(/[\\`]/),
          ),
        ),
        '\'',
      ),

    unquoted_string: ($) =>
      repeat1(
        choice(
          token.immediate(/[^\s\n\"'\\`\$]+/),
          token.immediate(/[\\`] /),
          token.immediate(/[\\`][^\s\n]/),
          $._immediate_expansion,
          // A `$` not starting a valid expansion is literal text. The first
          // form matches `$` plus the following non-identifier/brace char that
          // is not a value terminator (handles $5, $$, cost:$5.00); the second
          // matches a `$` at the end of the value. A real expansion still wins
          // for $ident and ${...}.
          token.immediate(/\$[^a-zA-Z_{\s"']/),
        ),
      ),

    _spaced_label_value: ($) =>
      spacedValue(
        $._spaced_label_value_atom,
        $._spaced_label_value_continuation,
      ),

    _continued_spaced_label_value: ($) =>
      continuedSpacedValue(
        $._spaced_label_value_continuation,
      ),

    _spaced_label_value_continuation: ($) =>
      spacedValueContinuation(
        $,
        $._spaced_label_value_atom,
      ),

    _spaced_label_value_atom: ($) =>
      choice($._spaced_label_value_fragment, $._non_newline_whitespace),

    _spaced_label_value_fragment: ($) =>
      choice(
        token.immediate(/[^\s\n\\`\$]+/),
        token.immediate(/[\\`] /),
        token.immediate(/[\\`][^\s\n]/),
        $._immediate_expansion,
      ),

    _spaced_env_value: ($) =>
      spacedValue(
        $._spaced_env_value_atom,
        $._spaced_env_value_continuation,
      ),

    _continued_spaced_env_value: ($) =>
      continuedSpacedValue(
        $._spaced_env_value_continuation,
      ),

    _spaced_env_value_continuation: ($) =>
      spacedValueContinuation(
        $,
        $._spaced_env_value_atom,
      ),

    _spaced_env_value_atom: ($) =>
      choice($._spaced_env_value_fragment, $._non_newline_whitespace),

    _spaced_env_value_fragment: ($) =>
      choice(
        token.immediate(/[^\s\n\"'\\`\$]+/),
        token.immediate(/[\\`] /),
        token.immediate(/[\\`][^\s\n]/),
        $._immediate_expansion,
      ),

    // The active escape char (\ by default, or ` via `# escape=`) escapes
    // ", \, `, and $ inside double-quoted strings. A \$ is literal text (the
    // following identifier is NOT expanded).
    double_quoted_escape_sequence: () => token.immediate(
      /[\\`][\\`"$]/,
    ),

    _non_newline_whitespace: () => token.immediate(/[\t ]+/),
  },
});
