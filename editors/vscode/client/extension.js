const path = require('path');
const vscode = require('vscode');
const { LanguageClient, TransportKind } = require('vscode-languageclient/node');

let client;

function activate(context) {
    const config = vscode.workspace.getConfiguration('foxlang');
    let serverPath = config.get('lsp.serverPath') || 'foxlang-lsp';

    // If serverPath is relative or a filename, check if build/foxlang-lsp exists in workspace
    if (serverPath === 'foxlang-lsp' && vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
        const workspaceRoot = vscode.workspace.workspaceFolders[0].uri.fsPath;
        const localBuildLsp = path.join(workspaceRoot, 'build', 'foxlang-lsp');
        const fs = require('fs');
        if (fs.existsSync(localBuildLsp)) {
            serverPath = localBuildLsp;
        }
    }

    const serverOptions = {
        run: {
            command: serverPath,
            transport: TransportKind.stdio
        },
        debug: {
            command: serverPath,
            transport: TransportKind.stdio
        }
    };

    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'fox' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.fox')
        },
        outputChannelName: 'FoxLang Language Server'
    };

    client = new LanguageClient(
        'foxlang',
        'FoxLang Language Server',
        serverOptions,
        clientOptions
    );

    client.start();
}

function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}

module.exports = {
    activate,
    deactivate
};
