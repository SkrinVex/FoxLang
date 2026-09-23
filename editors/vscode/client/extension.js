const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const vscode = require('vscode');

let serverProcess = null;
let diagnosticCollection = null;
let outputChannel = null;
let messageBuffer = Buffer.alloc(0);
let requestId = 1;
const pendingRequests = new Map();

function log(msg) {
    if (outputChannel) {
        outputChannel.appendLine(`[FoxLang LSP] ${msg}`);
    }
}

function resolveServerPath() {
    const config = vscode.workspace.getConfiguration('foxlang');
    let customPath = config.get('lsp.serverPath');
    if (customPath && customPath !== 'foxlang-lsp') {
        return customPath;
    }

    // Check workspace local build first
    if (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
        for (const folder of vscode.workspace.workspaceFolders) {
            const localBin = path.join(folder.uri.fsPath, 'build', 'foxlang-lsp');
            if (fs.existsSync(localBin)) {
                return localBin;
            }
            const localBinWin = path.join(folder.uri.fsPath, 'build', 'foxlang-lsp.exe');
            if (fs.existsSync(localBinWin)) {
                return localBinWin;
            }
        }
    }

    // Check common installation paths
    const home = process.env.HOME || process.env.USERPROFILE || '';
    const candidatePaths = [
        path.join(home, '.local', 'bin', 'foxlang-lsp'),
        path.join(home, '.local', 'share', 'foxlang', 'foxlang-lsp'),
        '/usr/local/bin/foxlang-lsp',
        '/usr/bin/foxlang-lsp'
    ];

    for (const p of candidatePaths) {
        if (p && fs.existsSync(p)) {
            return p;
        }
    }

    return 'foxlang-lsp';
}

function sendMessage(msg) {
    if (!serverProcess || !serverProcess.stdin || serverProcess.stdin.destroyed) {
        return;
    }
    const json = JSON.stringify(msg);
    const body = Buffer.from(json, 'utf8');
    const header = Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii');
    serverProcess.stdin.write(header);
    serverProcess.stdin.write(body);
}

function sendRequest(method, params) {
    return new Promise((resolve, reject) => {
        if (!serverProcess) {
            return reject(new Error('Server not running'));
        }
        const id = requestId++;
        pendingRequests.set(id, { resolve, reject });
        sendMessage({ jsonrpc: '2.0', id, method, params });
    });
}

function handleMessage(msg) {
    if (!msg || typeof msg !== 'object') return;

    // Handle responses to requests
    if (msg.id !== undefined && pendingRequests.has(msg.id)) {
        const { resolve, reject } = pendingRequests.get(msg.id);
        pendingRequests.delete(msg.id);
        if (msg.error) {
            reject(new Error(msg.error.message || 'LSP Request Error'));
        } else {
            resolve(msg.result);
        }
        return;
    }

    // Handle notifications from server
    if (msg.method === 'textDocument/publishDiagnostics') {
        const params = msg.params;
        if (!params || !params.uri) return;
        const uri = vscode.Uri.parse(params.uri);
        const diagnostics = (params.diagnostics || []).map(d => {
            const startLine = Math.max(0, (d.range && d.range.start ? d.range.start.line : 0));
            const startChar = Math.max(0, (d.range && d.range.start ? d.range.start.character : 0));
            const endLine = Math.max(0, (d.range && d.range.end ? d.range.end.line : startLine));
            const endChar = Math.max(0, (d.range && d.range.end ? d.range.end.character : startChar + 1));

            const range = new vscode.Range(startLine, startChar, endLine, endChar);
            let severity = vscode.DiagnosticSeverity.Error;
            if (d.severity === 2) severity = vscode.DiagnosticSeverity.Warning;
            else if (d.severity === 3) severity = vscode.DiagnosticSeverity.Information;
            else if (d.severity === 4) severity = vscode.DiagnosticSeverity.Hint;

            const diag = new vscode.Diagnostic(range, d.message, severity);
            diag.source = 'foxlang';
            return diag;
        });

        diagnosticCollection.set(uri, diagnostics);
    }
}

function startServer(context) {
    const serverPath = resolveServerPath();
    log(`Starting FoxLang LSP server: ${serverPath}`);

    try {
        serverProcess = cp.spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe']
        });
    } catch (err) {
        log(`Failed to spawn server process: ${err.message}`);
        vscode.window.showErrorMessage(`Не удалось запустить foxlang-lsp: ${err.message}`);
        return;
    }

    serverProcess.on('error', err => {
        log(`Server error: ${err.message}`);
        vscode.window.showErrorMessage(`Ошибка foxlang-lsp: ${err.message}. Проверьте путь в настройках.`);
    });

    serverProcess.on('exit', (code, signal) => {
        log(`Server exited with code: ${code}, signal: ${signal}`);
        serverProcess = null;
    });

    serverProcess.stderr.on('data', chunk => {
        const text = chunk.toString('utf8');
        log(text.trim());
    });

    messageBuffer = Buffer.alloc(0);
    serverProcess.stdout.on('data', chunk => {
        messageBuffer = Buffer.concat([messageBuffer, chunk]);
        while (true) {
            const headerEnd = messageBuffer.indexOf('\r\n\r\n');
            if (headerEnd === -1) break;

            const header = messageBuffer.slice(0, headerEnd).toString('ascii');
            const match = header.match(/Content-Length:\s*(\d+)/i);
            if (!match) {
                // Invalid header, discard
                messageBuffer = messageBuffer.slice(headerEnd + 4);
                continue;
            }

            const contentLength = parseInt(match[1], 10);
            const totalLength = headerEnd + 4 + contentLength;
            if (messageBuffer.length < totalLength) break;

            const bodyBytes = messageBuffer.slice(headerEnd + 4, totalLength);
            messageBuffer = messageBuffer.slice(totalLength);

            try {
                const parsed = JSON.parse(bodyBytes.toString('utf8'));
                handleMessage(parsed);
            } catch (err) {
                log(`JSON parse error: ${err.message}`);
            }
        }
    });

    // Initialize handshake
    const rootUri = (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0)
        ? vscode.workspace.workspaceFolders[0].uri.toString()
        : null;

    sendRequest('initialize', {
        processId: process.pid,
        rootUri: rootUri,
        capabilities: {
            textDocument: {
                synchronization: { dynamicRegistration: false },
                completion: { dynamicRegistration: false },
                signatureHelp: { dynamicRegistration: false },
                hover: { dynamicRegistration: false },
                definition: { dynamicRegistration: false },
                documentSymbol: { dynamicRegistration: false }
            }
        }
    }).then(res => {
        log('Server initialized successfully');
        sendMessage({ jsonrpc: '2.0', method: 'initialized', params: {} });

        // Synchronize currently opened Fox documents
        vscode.workspace.textDocuments.forEach(doc => {
            if (doc.languageId === 'fox') {
                syncDocumentOpen(doc);
            }
        });
    }).catch(err => {
        log(`Initialize request failed: ${err.message}`);
    });
}

function syncDocumentOpen(document) {
    if (document.languageId !== 'fox') return;
    sendMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: document.uri.toString(),
                languageId: 'fox',
                version: document.version,
                text: document.getText()
            }
        }
    });
}

function syncDocumentChange(event) {
    if (event.document.languageId !== 'fox') return;
    sendMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: {
                uri: event.document.uri.toString(),
                version: event.document.version
            },
            contentChanges: [
                { text: event.document.getText() }
            ]
        }
    });
}

function syncDocumentClose(document) {
    if (document.languageId !== 'fox') return;
    diagnosticCollection.delete(document.uri);
    sendMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didClose',
        params: {
            textDocument: {
                uri: document.uri.toString()
            }
        }
    });
}

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('FoxLang Language Server');
    diagnosticCollection = vscode.languages.createDiagnosticCollection('foxlang');
    context.subscriptions.push(diagnosticCollection);
    context.subscriptions.push(outputChannel);

    startServer(context);

    // Document synchronization events
    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(doc => syncDocumentOpen(doc)));
    context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(e => syncDocumentChange(e)));
    context.subscriptions.push(vscode.workspace.onDidCloseTextDocument(doc => syncDocumentClose(doc)));

    // Hover Provider
    context.subscriptions.push(vscode.languages.registerHoverProvider('fox', {
        async provideHover(document, position) {
            try {
                const res = await sendRequest('textDocument/hover', {
                    textDocument: { uri: document.uri.toString() },
                    position: { line: position.line, character: position.character }
                });
                if (!res || !res.contents) return null;
                const text = typeof res.contents === 'string' ? res.contents : (res.contents.value || '');
                if (!text) return null;
                return new vscode.Hover(new vscode.MarkdownString(text));
            } catch {
                return null;
            }
        }
    }));

    // Completion Provider
    context.subscriptions.push(vscode.languages.registerCompletionItemProvider('fox', {
        async provideCompletionItems(document, position) {
            try {
                const res = await sendRequest('textDocument/completion', {
                    textDocument: { uri: document.uri.toString() },
                    position: { line: position.line, character: position.character }
                });
                if (!res) return null;
                const items = Array.isArray(res) ? res : (res.items || []);
                return items.map(item => {
                    const ci = new vscode.CompletionItem(item.label);
                    ci.detail = item.detail;
                    if (item.documentation) {
                        const doc = typeof item.documentation === 'string' ? item.documentation : item.documentation.value;
                        ci.documentation = new vscode.MarkdownString(doc);
                    }
                    if (item.kind) {
                        ci.kind = item.kind - 1;
                    }
                    return ci;
                });
            } catch {
                return null;
            }
        }
    }, '.', ' '));

    // Signature Help Provider (Fires on '(' and ',')
    context.subscriptions.push(vscode.languages.registerSignatureHelpProvider('fox', {
        async provideSignatureHelp(document, position) {
            try {
                const res = await sendRequest('textDocument/signatureHelp', {
                    textDocument: { uri: document.uri.toString() },
                    position: { line: position.line, character: position.character }
                });
                if (!res || !res.signatures || res.signatures.length === 0) return null;
                const sh = new vscode.SignatureHelp();
                sh.activeSignature = res.activeSignature || 0;
                sh.activeParameter = res.activeParameter || 0;
                sh.signatures = res.signatures.map(s => {
                    const sig = new vscode.SignatureInformation(s.label);
                    if (s.documentation) {
                        const doc = typeof s.documentation === 'string' ? s.documentation : (s.documentation.value || '');
                        sig.documentation = new vscode.MarkdownString(doc);
                    }
                    if (Array.isArray(s.parameters)) {
                        sig.parameters = s.parameters.map(p => {
                            const pi = new vscode.ParameterInformation(p.label);
                            if (p.documentation) {
                                const pdoc = typeof p.documentation === 'string' ? p.documentation : (p.documentation.value || '');
                                pi.documentation = new vscode.MarkdownString(pdoc);
                            }
                            return pi;
                        });
                    }
                    return sig;
                });
                return sh;
            } catch {
                return null;
            }
        }
    }, '(', ','));

    // Definition Provider (F12 / Ctrl+Click)
    context.subscriptions.push(vscode.languages.registerDefinitionProvider('fox', {
        async provideDefinition(document, position) {
            try {
                const res = await sendRequest('textDocument/definition', {
                    textDocument: { uri: document.uri.toString() },
                    position: { line: position.line, character: position.character }
                });
                if (!res) return null;
                const locations = Array.isArray(res) ? res : [res];
                return locations.map(loc => {
                    const targetUri = vscode.Uri.parse(loc.uri);
                    const range = new vscode.Range(
                        loc.range.start.line,
                        loc.range.start.character,
                        loc.range.end.line,
                        loc.range.end.character
                    );
                    return new vscode.Location(targetUri, range);
                });
            } catch {
                return null;
            }
        }
    }));

    // Document Symbols Provider (Ctrl+Shift+O)
    context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider('fox', {
        async provideDocumentSymbols(document) {
            try {
                const res = await sendRequest('textDocument/documentSymbol', {
                    textDocument: { uri: document.uri.toString() }
                });
                if (!res || !Array.isArray(res)) return null;

                function convertSymbol(s) {
                    const range = new vscode.Range(
                        s.range.start.line,
                        s.range.start.character,
                        s.range.end.line,
                        s.range.end.character
                    );
                    const selRange = s.selectionRange ? new vscode.Range(
                        s.selectionRange.start.line,
                        s.selectionRange.start.character,
                        s.selectionRange.end.line,
                        s.selectionRange.end.character
                    ) : range;

                    const kind = s.kind ? (s.kind - 1) : vscode.SymbolKind.Variable;
                    const docSymbol = new vscode.DocumentSymbol(s.name, s.detail || '', kind, range, selRange);
                    if (s.children && Array.isArray(s.children)) {
                        docSymbol.children = s.children.map(convertSymbol);
                    }
                    return docSymbol;
                }

                return res.map(convertSymbol);
            } catch {
                return null;
            }
        }
    }));

    // Command to restart LSP
    context.subscriptions.push(vscode.commands.registerCommand('foxlang.restartServer', () => {
        if (serverProcess) {
            try { serverProcess.kill(); } catch {}
            serverProcess = null;
        }
        startServer(context);
        vscode.window.showInformationMessage('FoxLang Language Server перезапущен.');
    }));
}

function deactivate() {
    if (serverProcess) {
        try {
            sendMessage({ jsonrpc: '2.0', method: 'shutdown', id: requestId++ });
            setTimeout(() => {
                if (serverProcess) {
                    try { serverProcess.kill(); } catch {}
                }
            }, 500);
        } catch {}
    }
    if (diagnosticCollection) {
        diagnosticCollection.clear();
    }
}

module.exports = {
    activate,
    deactivate
};
