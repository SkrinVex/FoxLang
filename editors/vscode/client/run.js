// Run, check and build FoxLang programs from VS Code: the ▶ button in the editor
// title, commands, and a "foxlang" task type for tasks.json.
const fs = require('fs');
const os = require('os');
const path = require('path');
const vscode = require('vscode');

const TASK_TYPE = 'foxlang';

function config() {
    return vscode.workspace.getConfiguration('foxlang');
}

function resolveExecutable() {
    const custom = config().get('run.executablePath');
    if (custom && custom !== 'foxlang') return custom;
    const name = process.platform === 'win32' ? 'foxlang.exe' : 'foxlang';
    const candidates = [];
    for (const folder of vscode.workspace.workspaceFolders || []) {
        candidates.push(path.join(folder.uri.fsPath, 'build', name));
        candidates.push(path.join(folder.uri.fsPath, 'build', 'Release', name));
    }
    candidates.push(path.join(os.homedir(), '.local', 'bin', name));
    for (const candidate of candidates) {
        if (fs.existsSync(candidate)) return candidate;
    }
    return 'foxlang';
}

// The program to run: foxlang.run.entryFile when it is set (a project whose main file
// includes the others), otherwise the file open in the editor.
function entryFile() {
    const entry = config().get('run.entryFile');
    if (entry) {
        if (path.isAbsolute(entry)) return entry;
        const editor = vscode.window.activeTextEditor;
        const folder = (editor && vscode.workspace.getWorkspaceFolder(editor.document.uri)) ||
            (vscode.workspace.workspaceFolders || [])[0];
        if (folder) return path.join(folder.uri.fsPath, entry);
    }
    const editor = vscode.window.activeTextEditor;
    if (editor && editor.document.languageId === 'fox' && editor.document.uri.scheme === 'file') {
        return editor.document.uri.fsPath;
    }
    return null;
}

function makeTask(command, file, extraArgs) {
    const executable = resolveExecutable();
    let args;
    if (command === 'check') {
        args = ['check', file];
    } else if (command === 'build') {
        args = ['build', file, '-o', path.basename(file, '.fox')];
    } else {
        args = [file, ...(extraArgs || config().get('run.arguments') || [])];
    }
    const definition = { type: TASK_TYPE, command, file };
    // Full paths in error messages let the problem matcher find files from any directory.
    const execution = new vscode.ProcessExecution(executable, args, {
        cwd: path.dirname(file),
        env: { FOXLANG_ABSOLUTE_PATHS: '1' }
    });
    const labels = { run: 'Запуск', check: 'Проверка', build: 'Сборка' };
    const task = new vscode.Task(definition, vscode.TaskScope.Workspace,
        `${labels[command] || command}: ${path.basename(file)}`, 'FoxLang', execution,
        command === 'check' ? ['$foxlang'] : ['$foxlang-runtime']);
    task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, clear: true, focus: command === 'run' };
    if (command === 'build') task.group = vscode.TaskGroup.Build;
    return task;
}

async function start(command) {
    const file = entryFile();
    if (!file) {
        vscode.window.showWarningMessage('Откройте файл .fox или укажите главный файл командой «FoxLang: выбрать главный файл».');
        return;
    }
    // Every file of a project may be part of the program, so all of them are saved.
    await vscode.workspace.saveAll(false);
    await vscode.tasks.executeTask(makeTask(command, file));
}

async function chooseEntry() {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'fox') {
        vscode.window.showWarningMessage('Откройте главный файл программы .fox.');
        return;
    }
    const folder = vscode.workspace.getWorkspaceFolder(editor.document.uri);
    const value = folder ? path.relative(folder.uri.fsPath, editor.document.uri.fsPath) : editor.document.uri.fsPath;
    await config().update('run.entryFile', value, folder ? vscode.ConfigurationTarget.WorkspaceFolder : vscode.ConfigurationTarget.Global);
    vscode.window.showInformationMessage(`Главный файл FoxLang: ${value}`);
}

function activate(context) {
    context.subscriptions.push(vscode.commands.registerCommand('foxlang.run', () => start('run')));
    context.subscriptions.push(vscode.commands.registerCommand('foxlang.check', () => start('check')));
    context.subscriptions.push(vscode.commands.registerCommand('foxlang.build', () => start('build')));
    context.subscriptions.push(vscode.commands.registerCommand('foxlang.chooseEntry', chooseEntry));
    context.subscriptions.push(vscode.tasks.registerTaskProvider(TASK_TYPE, {
        provideTasks() {
            const file = entryFile();
            return file ? ['run', 'check', 'build'].map(command => makeTask(command, file)) : [];
        },
        resolveTask(task) {
            const definition = task.definition;
            if (!definition.file) return undefined;
            let file = definition.file;
            const folder = (vscode.workspace.workspaceFolders || [])[0];
            if (!path.isAbsolute(file) && folder) file = path.join(folder.uri.fsPath, file);
            const resolved = makeTask(definition.command || 'run', file, definition.args);
            resolved.definition = definition;
            return resolved;
        }
    }));
}

module.exports = { activate };
