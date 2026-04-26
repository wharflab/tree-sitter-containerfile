((comment) @injection.content
  (#set! injection.language "comment"))

((shell_command) @injection.content
  (#set! injection.language "bash")
  (#set! injection.combined))

((run_instruction
  (heredoc_block) @injection.content)
  (#set! injection.language "bash")
  (#set! injection.include-children))
