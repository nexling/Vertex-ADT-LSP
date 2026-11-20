#!/usr/bin/env node
'use strict';

/**
 * Parity goldens for workspace CSV exports (`workspace/executeCommand`
 * `vertexadt.exportWorkspaceReports` / `vertexadt.exportWorkspaceDiagnostics`).
 * Assertions from Specify workspace CSV exports and the fixtures it cites:
 *   test-fixtures/workspace-declaration-index/ scenarios 10 and 18
 *     (`shared_callee.prg` call_count 2, `shared_include.prg` include_count 2,
 *     `CustomWins=99` count 1 and no `SystemOnly`, header-only local slots;
 *     strict-on Scenario 1 information rows with `related_program` the other
 *     file, gone with strict off, no System-twin path; `exportCustomOnly`)
 *   test-fixtures/local-slot-names/ scenario 7 (callee local-slot rows).
 *
 * Oracle: the live Node language server (`custom/export*` requests). Native
 * and Node each get their own no-spaces temp copy of the fixture (Node's
 * findFile does not decode `%20`), and the CSV files they write are compared
 * byte for byte. Two extra temp files exercise the System-only gate and the
 * 10000 problem cap.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURES = path.join(ROOT, 'test-fixtures');
const NODE_SERVER = path.join(ROOT, 'server', 'out', 'server.js');

const REPORTS = 'vertexadt.exportWorkspaceReports';
const DIAGNOSTICS = 'vertexadt.exportWorkspaceDiagnostics';
const REPORT_FILES = {
    programUsagePath: 'vertexadt-program-usage.csv',
    declarationNumbersPath: 'vertexadt-declaration-numbers.csv',
    localSlotsPath: 'vertexadt-local-slots.csv',
};
const DIAGNOSTICS_FILE = 'vertexadt-workspace-diagnostics.csv';
const DIAGNOSTICS_HEADER = 'program,line,column,severity,code,source,message,related_program';
const BASE_SETTINGS = { maxNumberOfProblems: 100 };

class AssertionError extends Error {
    constructor(message) {
        super(message);
        this.name = 'AssertionError';
    }
}

function assert(condition, message) {
    if (!condition) {
        throw new AssertionError(message);
    }
}

function assertEqual(actual, expected, message) {
    const a = JSON.stringify(actual);
    const e = JSON.stringify(expected);
    assert(a === e, `${message}: expected ${e}, got ${a}`);
}

function acceptRefreshes(session) {
    const base = session._replyToServerRequest.bind(session);
    session._replyToServerRequest = (message) => {
        if (/^workspace\/.*\/refresh$/.test(message.method || '')) {
            session._write({ jsonrpc: '2.0', id: message.id, result: null });
            return;
        }
        base(message);
    };
}

class NodeOracleSession extends LspSession {
    static launchNode(options) {
        const child = spawn(process.execPath, [NODE_SERVER, '--stdio'], {
            cwd: ROOT,
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });
        child.stdin.setDefaultEncoding('utf8');
        const session = new NodeOracleSession(child, options);
        session.settings = { ...BASE_SETTINGS };
        return session;
    }

    async initialize() {
        const result = await this.request('initialize', {
            processId: process.pid,
            rootUri: this._rootUri,
            capabilities: {
                workspace: { configuration: true, workspaceFolders: true },
                textDocument: { publishDiagnostics: { relatedInformation: true } },
            },
            initializationOptions: {
                extensionUri: this._extensionUri,
                settings: { settings: this.settings },
            },
            workspaceFolders: this._workspaceFolders,
        });
        this.notify('initialized', {});
        return result;
    }

    // Node keeps the host's flattened shape: `strictMode` is a boolean.
    update(settings) {
        this.settings = { ...this.settings, ...settings };
        this.notify('custom/updateSettings', settings);
    }

    _replyToServerRequest(message) {
        if (message.method === 'workspace/configuration') {
            const items = (message.params && message.params.items) || [];
            this._write({ jsonrpc: '2.0', id: message.id, result: items.map(() => this.settings) });
            return;
        }
        super._replyToServerRequest(message);
    }
}

function copyFixture(name, extraFiles = {}) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vertexadt-47-'));
    assert(!dir.includes(' '), `temp fixture path has no spaces: ${dir}`);
    for (const tree of ['custom', 'system']) {
        const source = path.join(FIXTURES, name, tree);
        if (fs.existsSync(source)) {
            fs.cpSync(source, path.join(dir, tree), { recursive: true });
        }
    }
    for (const [relative, text] of Object.entries(extraFiles)) {
        fs.mkdirSync(path.dirname(path.join(dir, relative)), { recursive: true });
        fs.writeFileSync(path.join(dir, relative), text, 'utf8');
    }
    return dir;
}

function sessionOptions(root) {
    const rootUri = pathToFileURL(root).href;
    return {
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: path.basename(root) }],
    };
}

// One native + one Node session, each on its own temp copy.
async function withPair(fixture, extraFiles, fn) {
    const nativeRoot = copyFixture(fixture, extraFiles);
    const nodeRoot = copyFixture(fixture, extraFiles);
    const native = LspSession.launch(sessionOptions(nativeRoot));
    const oracle = NodeOracleSession.launchNode(sessionOptions(nodeRoot));
    acceptRefreshes(native);
    acceptRefreshes(oracle);
    try {
        await native.initialize();
        native.didChangeConfiguration({ vertexadt: BASE_SETTINGS });
        await oracle.initialize();
        await native.waitQuiet(300);
        await oracle.waitQuiet(300);
        await fn({ native, oracle, nativeRoot, nodeRoot });
        await native.shutdownAndExit();
        oracle.kill();
    } catch (err) {
        native.kill();
        oracle.kill();
        throw err;
    } finally {
        fs.rmSync(nativeRoot, { recursive: true, force: true });
        fs.rmSync(nodeRoot, { recursive: true, force: true });
    }
}

const execute = (session, command) =>
    session.request('workspace/executeCommand', { command }, 60000);

function readCsv(file) {
    const bytes = fs.readFileSync(file);
    assert(!(bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf), `${file}: no BOM`);
    const text = bytes.toString('utf8');
    assert(text.endsWith('\n'), `${file}: trailing newline`);
    assert(!text.includes('\r\n'), `${file}: \\n rows`);
    return text;
}

const rows = (text) => text.split('\n').slice(1, -1);

async function exportReports(native, oracle, nativeRoot, nodeRoot, label) {
    const ours = await execute(native, REPORTS);
    const theirs = await oracle.request('custom/exportWorkspaceReports', undefined, 60000);
    assertEqual(Object.keys(ours).sort(), Object.keys(REPORT_FILES).sort(), `${label}: result keys`);
    const texts = {};
    for (const [key, name] of Object.entries(REPORT_FILES)) {
        assert(ours[key] === path.join(nativeRoot, name), `${label}: ${key} is ${ours[key]}`);
        assert(theirs[key] === path.join(nodeRoot, name), `${label}: Node ${key} is ${theirs[key]}`);
        const text = readCsv(ours[key]);
        assert(text === readCsv(theirs[key]), `${label}: ${name} native vs Node\n--- native\n${text}--- node\n${readCsv(theirs[key])}`);
        texts[key] = text;
    }
    return texts;
}

async function exportDiagnostics(native, oracle, nativeRoot, nodeRoot, label) {
    const ours = await execute(native, DIAGNOSTICS);

    const theirs = await oracle.request('custom/exportWorkspaceDiagnostics', undefined, 180000);
    assert(ours.diagnosticsPath === path.join(nativeRoot, DIAGNOSTICS_FILE), `${label}: path ${ours.diagnosticsPath}`);
    const text = readCsv(ours.diagnosticsPath);
    const nodeText = readCsv(theirs.diagnosticsPath);
    if (text !== nodeText) {
        const a = text.split('\n');
        const b = nodeText.split('\n');
        const diff = [];
        for (let i = 0; i < Math.max(a.length, b.length) && diff.length < 12; i++) {
            if (a[i] !== b[i]) {
                diff.push(`row ${i}\n  native: ${a[i]}\n  node:   ${b[i]}`);
            }
        }
        throw new AssertionError(
            `${label}: ${DIAGNOSTICS_FILE} native vs Node (${a.length} vs ${b.length} lines)\n${diff.join('\n')}`);
    }
    const { diagnosticsPath: _a, ...counts } = ours;
    const { diagnosticsPath: _b, ...nodeCounts } = theirs;
    assertEqual(counts, nodeCounts, `${label}: counts native vs Node`);
    assert(text.split('\n')[0] === DIAGNOSTICS_HEADER, `${label}: header`);
    assert(counts.diagnosticCount === rows(text).length, `${label}: diagnosticCount is the row count`);
    return { text, counts };
}

// Parse a CSV line (cells may be quoted with doubled quotes).
function cells(line) {
    const out = [];
    let i = 0;
    while (i <= line.length) {
        if (line[i] === '"') {
            let value = '';
            i++;
            while (i < line.length) {
                if (line[i] === '"' && line[i + 1] === '"') {
                    value += '"';
                    i += 2;
                } else if (line[i] === '"') {
                    i++;
                    break;
                } else {
                    value += line[i++];
                }
            }
            out.push(value);
            i++;
        } else {
            const end = line.indexOf(',', i);
            const stop = end === -1 ? line.length : end;
            out.push(line.slice(i, stop));
            i = stop + 1;
        }
    }
    return out;
}

async function declarationIndexGoldens() {
    const extraFiles = {
        // System-only: validated only with exportCustomOnly false.
        'system/system_only_export.prg': 'if a = 1 then\n',
        // More problems than the live editor cap of 100.
        'custom/many_problems_export.prg': Array.from({ length: 150 }, () => 'endif\n').join(''),
    };
    await withPair('workspace-declaration-index', extraFiles, async ({ native, oracle, nativeRoot, nodeRoot }) => {
        const init = native.initializeResult;
        assertEqual(init.capabilities.executeCommandProvider, { commands: [] },
            'executeCommandProvider keeps an empty commands list');

        // Reports.
        const reports = await exportReports(native, oracle, nativeRoot, nodeRoot, 'reports');
        const usage = rows(reports.programUsagePath);
        assert(reports.programUsagePath.startsWith('program,call_count,include_count\n'), 'program usage header');
        assert(usage.includes('shared_callee.prg,2,0'), `shared_callee call_count 2: ${usage}`);
        assert(usage.some((row) => /^shared_include\.prg,\d+,2$/.test(row)), `shared_include include_count 2: ${usage}`);
        assert(!usage.some((row) => row.startsWith('custom/') || row.startsWith('system/')),
            'program is the library-relative display path');
        const declarations = rows(reports.declarationNumbersPath);
        assert(reports.declarationNumbersPath.startsWith('variable_name,declaration_number,count\n'),
            'declaration numbers header');
        assert(declarations.includes('CustomWins,99,1'), `CustomWins=99 count 1: ${declarations}`);
        assert(!declarations.some((row) => row.startsWith('SystemOnly,')), 'System twin not counted');
        assert(!declarations.some((row) => /^DUP[AB],/.test(row)), 'no #EDIT_FORM# DUPA/DUPB');
        assert(reports.localSlotsPath === 'program,local_slot_name,declaration_number\n',
            `local slots header-only: ${JSON.stringify(reports.localSlotsPath)}`);

        // Diagnostics, strict off (default).
        const off = await exportDiagnostics(native, oracle, nativeRoot, nodeRoot, 'diagnostics strict off');
        const offRows = rows(off.text).map(cells);
        assert(!offRows.some((row) => row[0] === 'main_include.prg' && row[3] === 'information'),
            'strict off: no Scenario 1 information rows');
        assert(!offRows.some((row) => row[0].startsWith('system/') || row[0] === 'system_only_export.prg'),
            'exportCustomOnly default: no System rows');
        const many = offRows.filter((row) => row[0] === 'many_problems_export.prg');
        assert(many.length > 100, `export cap raised past 100: ${many.length} rows`);

        // Strict on: Scenario 1 cross-file rows with related_program.
        native.didChangeConfiguration({ vertexadt: { ...BASE_SETTINGS, strictMode: { enabled: true } } });
        oracle.update({ strictMode: true });
        await native.waitQuiet(400);
        await oracle.waitQuiet(400);
        const on = await exportDiagnostics(native, oracle, nativeRoot, nodeRoot, 'diagnostics strict on');
        const onRows = rows(on.text).map(cells);
        const scenario1 = (program, other) => onRows.some((row) =>
            row[0] === program && row[3] === 'information' && row[7] === other);
        assert(scenario1('main_include.prg', 'sub_included.prg'), 'main_include information row -> sub_included');
        assert(scenario1('sub_included.prg', 'main_include.prg'), 'sub_included information row -> main_include');
        assert(!onRows.some((row) => row[0].startsWith('system/') || row[7].startsWith('system/')),
            'no System-twin path');
        assert(on.counts.informationCount > off.counts.informationCount, 'strict adds information rows');

        // exportCustomOnly false: System-only files join; twins stay Custom.
        native.didChangeConfiguration({
            vertexadt: { ...BASE_SETTINGS, strictMode: { enabled: true }, diagnostics: { exportCustomOnly: false } },
        });
        oracle.update({ diagnostics: { exportCustomOnly: false } });
        await native.waitQuiet(400);
        await oracle.waitQuiet(400);
        const all = await exportDiagnostics(native, oracle, nativeRoot, nodeRoot, 'diagnostics exportCustomOnly off');
        const allRows = rows(all.text).map(cells);
        assert(all.counts.fileCount === on.counts.fileCount + 1, `System-only file counted: ${all.counts.fileCount}`);
        assert(allRows.some((row) => row[0] === 'system_only_export.prg'), 'System-only file has rows');
        assert(!allRows.some((row) => row[0] === 'system/custom_override.prg'), 'System twin still skipped');

        // The live editor cap is unchanged after export.
        const manyUri = pathToFileURL(path.join(nativeRoot, 'custom', 'many_problems_export.prg')).href;
        native.didOpen({ uri: manyUri, languageId: 'vertexadt', version: 1,
            text: fs.readFileSync(path.join(nativeRoot, 'custom', 'many_problems_export.prg'), 'utf8') });
        const pulled = await native.diagnostic(manyUri);
        assert(pulled.items.length <= 101, `live cap restored: ${pulled.items.length} items`);
        native.didClose(manyUri);
    });
}

async function localSlotGoldens() {
    await withPair('local-slot-names', {}, async ({ native, oracle, nativeRoot, nodeRoot }) => {
        const reports = await exportReports(native, oracle, nativeRoot, nodeRoot, 'local-slot-names reports');
        const slots = rows(reports.localSlotsPath).map(cells);
        assert(slots.length > 0, 'callee local-slot rows');
        assert(slots.some((row) => row[0] === 'callee_a.prg' && row[1].startsWith('l1')), `callee_a l1 rows: ${slots}`);
        assert(!slots.some((row) => row[0] === 'redeclare.prg'), 'non-callee has no rows');
    });
}

async function noRootGolden() {
    // initialize always sends rootUri; point it at an empty temp dir so a
    // wrong fallback cannot write into the repo. No workspace folders.
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'vertexadt-47-noroot-'));
    const session = LspSession.launch({
        rootUri: pathToFileURL(dir).href,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [],
    });
    acceptRefreshes(session);
    try {
        await session.initialize();
        for (const command of [REPORTS, DIAGNOSTICS]) {
            assertEqual(await execute(session, command), { error: 'No workspace root found' }, `${command} with no folders`);
        }
        assertEqual(fs.readdirSync(dir), [], 'nothing written with no folders');
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    } finally {
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

async function run() {
    await declarationIndexGoldens();
    await localSlotGoldens();
    await noRootGolden();
    console.log('lsp golden: 47-workspace-csv-exports passed');
}

module.exports = { run };

if (require.main === module) {
    run().catch((err) => {
        console.error(err);
        process.exit(1);
    });
}
