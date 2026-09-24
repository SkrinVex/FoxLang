use zed_extension_api::{
    self as zed, serde_json, Command, DebugAdapterBinary, DebugConfig, DebugRequest, DebugScenario,
    DebugTaskDefinition, LanguageServerId, Result, StartDebuggingRequestArguments,
    StartDebuggingRequestArgumentsRequest, Worktree,
};

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

    // The debugger is `foxlang debug-adapter`, speaking DAP over stdin/stdout; the
    // program's output arrives in the debug console.
    fn get_dap_binary(
        &mut self,
        adapter_name: String,
        config: DebugTaskDefinition,
        user_provided_debug_adapter_path: Option<String>,
        worktree: &Worktree,
    ) -> Result<DebugAdapterBinary, String> {
        let command = match user_provided_debug_adapter_path {
            Some(path) => path,
            None => worktree
                .which("foxlang")
                .ok_or_else(|| "foxlang executable not found in PATH. Please install FoxLang 6.4 or newer.".to_string())?,
        };
        let parsed: serde_json::Value = serde_json::from_str(&config.config)
            .map_err(|error| format!("Invalid FoxLang debug configuration: {error}"))?;
        let request = self.dap_request_kind(adapter_name, parsed)?;
        Ok(DebugAdapterBinary {
            command: Some(command),
            arguments: vec!["debug-adapter".to_string()],
            envs: Vec::new(),
            cwd: Some(worktree.root_path()),
            connection: None,
            request_args: StartDebuggingRequestArguments {
                configuration: config.config,
                request,
            },
        })
    }

    fn dap_request_kind(
        &mut self,
        _adapter_name: String,
        config: serde_json::Value,
    ) -> Result<StartDebuggingRequestArgumentsRequest, String> {
        match config.get("request").and_then(|value| value.as_str()) {
            Some("launch") => Ok(StartDebuggingRequestArgumentsRequest::Launch),
            Some("attach") => Err("FoxLang starts the program itself: use \"request\": \"launch\"".to_string()),
            _ => Err("FoxLang debug configuration needs \"request\": \"launch\"".to_string()),
        }
    }

    fn dap_config_to_scenario(&mut self, config: DebugConfig) -> Result<DebugScenario, String> {
        match config.request {
            DebugRequest::Launch(launch) => {
                let mut settings = serde_json::json!({
                    "request": "launch",
                    "program": launch.program,
                    "args": launch.args,
                    "stopOnEntry": config.stop_on_entry.unwrap_or(false),
                });
                if let Some(cwd) = launch.cwd {
                    settings["cwd"] = serde_json::Value::String(cwd);
                }
                Ok(DebugScenario {
                    label: config.label,
                    adapter: config.adapter,
                    build: None,
                    config: settings.to_string(),
                    tcp_connection: None,
                })
            }
            DebugRequest::Attach(_) => Err("FoxLang starts the program itself: attaching is not supported".to_string()),
        }
    }
}

zed::register_extension!(FoxLangExtension);
