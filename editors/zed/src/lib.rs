use zed_extension_api::{self as zed, Command, LanguageServerId, Result, Worktree};

struct FoxLangExtension;

impl zed::Extension for FoxLangExtension {
    fn new() -> Self {
        Self
    }

    fn language_server_command(
        &mut self,
        _language_server_id: &LanguageServerId,
        worktree: &Worktree,
    ) -> Result<Command> {
        let path = worktree
            .which("foxlang-lsp")
            .ok_or_else(|| "foxlang-lsp executable not found in PATH. Please install FoxLang or ensure foxlang-lsp is in your PATH.".to_string())?;

        Ok(Command {
            command: path,
            args: vec!["--stdio".to_string()],
            env: Default::default(),
        })
    }
}

zed::register_extension!(FoxLangExtension);
