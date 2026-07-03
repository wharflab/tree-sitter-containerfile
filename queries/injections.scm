((comment) @injection.content
  (#set! injection.language "comment"))

; Each shell_command is its own bash document. Do NOT add
; (#set! injection.combined): it would parse every RUN body in the file as a
; single bash document, so the trailing token of one RUN fuses with the leading
; command word of the next (e.g. "migrations.sh" + "bundle"), breaking command
; highlighting from the second RUN onward. See issue #27.
((shell_command) @injection.content
  (#set! injection.language "bash"))

; Inject into the heredoc body only (heredoc_content), which excludes the
; closing delimiter line so the sub-language does not ingest the terminator.
((run_instruction
  (heredoc_block
    (heredoc_content) @injection.content))
  (#set! injection.language "bash"))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block
    (heredoc_content) @injection.content))
  (#match? @injection.filename "\\.[jJ][sS][oO][nN]$")
  (#set! injection.language "json"))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block
    (heredoc_content) @injection.content))
  (#match? @injection.filename "\\.[yY][aA]?[mM][lL]$")
  (#set! injection.language "yaml"))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block
    (heredoc_content) @injection.content))
  (#match? @injection.filename "\\.[tT][oO][mM][lL]$")
  (#set! injection.language "toml"))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block
    (heredoc_content) @injection.content))
  (#match? @injection.filename "\\.[xX][mM][lL]$")
  (#set! injection.language "xml"))
