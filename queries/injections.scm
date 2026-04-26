((comment) @injection.content
  (#set! injection.language "comment"))

((shell_command) @injection.content
  (#set! injection.language "bash")
  (#set! injection.combined))

((run_instruction
  (heredoc_block) @injection.content)
  (#set! injection.language "bash")
  (#set! injection.include-children))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block) @injection.content)
  (#match? @injection.filename "\\.[jJ][sS][oO][nN]$")
  (#set! injection.language "json")
  (#set! injection.include-children))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block) @injection.content)
  (#match? @injection.filename "\\.[yY][aA]?[mM][lL]$")
  (#set! injection.language "yaml")
  (#set! injection.include-children))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block) @injection.content)
  (#match? @injection.filename "\\.[tT][oO][mM][lL]$")
  (#set! injection.language "toml")
  (#set! injection.include-children))

((copy_instruction
  (param)*
  (path
    (heredoc_marker))
  .
  (path) @injection.filename
  .
  (heredoc_block) @injection.content)
  (#match? @injection.filename "\\.[xX][mM][lL]$")
  (#set! injection.language "xml")
  (#set! injection.include-children))
