'use strict';

/**
 * Native-only goldens for code-first macro library/code completion
 * (vertexadt.completion.macroCodeFirst,
 * docs/adr/0008-native-macro-code-first-completion.md).
 * Assertions from test-fixtures/macro-library-code-completion/TESTING.md
 * ("Code-first mode"). The Node server is frozen, so there is no oracle.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'macro-library-code-completion');
const MACRO_SYS = path.join(ROOT, 'test-fixtures', 'vertex-dbase-converter', 'custom', 'complibs', 'macro_sys');
const FOLDER_KIND = 19;
const VALUE_KIND = 12;
const TEXT_KIND = 1;
const XML_TIMEOUT_MS = 120000;

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
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

function copyDir(from, to) {
    fs.mkdirSync(to, { recursive: true });
    for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
        const source = path.join(from, entry.name);
        const target = path.join(to, entry.name);
        if (entry.isDirectory()) {
            copyDir(source, target);
        } else {
            fs.copyFileSync(source, target);
        }
    }
}

function writeLibraryXml(complibs, dir, table, codes) {
    const folder = path.join(complibs, dir);
    fs.mkdirSync(folder, { recursive: true });
    const records = codes
        .map((code, i) => `  <Record Index="${i + 1}"><Cell Name="CODE">${code}</Cell></Record>`)
        .join('\n');
    fs.writeFileSync(
        path.join(folder, `d_${table}.xml`),
        `<?xml version="1.0" encoding="utf-8"?>\n<VertexDatabase><Table Name="${table}"><Records>\n${records}\n</Records></Table></VertexDatabase>\n`
    );
}

function copyMacroSys(complibs) {
    const macroSys = path.join(complibs, 'macro_sys');
    fs.mkdirSync(macroSys, { recursive: true });
    for (const name of ['d_MACRO_SYS', 'd_MACRO_SYSc', 'd_MACRO_SYSl']) {
        fs.copyFileSync(path.join(MACRO_SYS, name), path.join(macroSys, name));
    }
}

// The 42 fixture plus `dup_lib` (shares BATHZONE with codes_lib) and the stale
// macro_sys database.
function makeWorkspace(root) {
    copyDir(FIXTURE, root);
    const custom = path.join(root, 'customa');
    const complibs = path.join(custom, 'complibs');
    copyMacroSys(complibs);
    writeLibraryXml(complibs, 'dup_lib', 'DUP_LIB', ['BATHZONE', 'DUPONLY']);
    const past = new Date(Date.now() - 3600 * 1000);
    fs.utimesSync(path.join(complibs, 'codes_lib', 'd_CODES_LIB'), past, past);
    fs.utimesSync(path.join(complibs, 'codes_lib', 'd_CODES_LIBc'), past, past);
    return workspaceOf(root);
}

// Only the stale macro_sys database: nothing listable before its export.
function makeExportOnlyWorkspace(root) {
    const complibs = path.join(root, 'customa', 'complibs');
    fs.mkdirSync(path.join(root, 'system'), { recursive: true });
    copyMacroSys(complibs);
    return workspaceOf(root);
}

function workspaceOf(root) {
    const custom = path.join(root, 'customa');
    return {
        root,
        custom,
        system: path.join(root, 'system'),
        complibs: path.join(custom, 'complibs'),
        probeUri: pathToFileURL(path.join(custom, 'probe.prg')).href,
    };
}

function sessionOptions(workspace) {
    return {
        rootUri: pathToFileURL(workspace.root).href,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [
            { uri: pathToFileURL(workspace.system).href, name: 'system' },
            { uri: pathToFileURL(workspace.custom).href, name: 'customa' },
        ],
    };
}

function setCodeFirst(session, enabled) {
    // configurationSection 'vertexadt', as the VS Code client sends it.
    session.didChangeConfiguration({ vertexadt: { completion: { macroCodeFirst: enabled } } });
}

// Cursor `|`; without one, the end of the text.
function splitCursor(textWithCursor) {
    const at = textWithCursor.indexOf('|');
    const text = at < 0 ? textWithCursor : textWithCursor.slice(0, at) + textWithCursor.slice(at + 1);
    const before = at < 0 ? text : text.slice(0, at);
    const lines = before.split('\n');
    return { text, line: lines.length - 1, character: lines[lines.length - 1].length };
}

class Probe {
    constructor(session, uri) {
        this.session = session;
        this.uri = uri;
        this.version = 0;
    }

    async complete(textWithCursor) {
        const { text, line, character } = splitCursor(textWithCursor);
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
        const result = await this.session.request('textDocument/completion', {
            textDocument: { uri: this.uri },
            position: { line, character },
            context: { triggerKind: 1 },
        }, XML_TIMEOUT_MS);
        return ranking.familyItemsOf(result, 'macroLibrary', textWithCursor);
    }

    // Retries until `done(items)` or the export timeout.
    async completeUntil(textWithCursor, done, message) {
        const deadline = Date.now() + XML_TIMEOUT_MS;
        for (;;) {
            const items = await this.complete(textWithCursor);
            if (done(items)) {
                return items;
            }
            assert(Date.now() < deadline, `${message}: timed out`);
            await new Promise((resolve) => setTimeout(resolve, 250));
        }
    }
}

const labels = (items) => items.map((item) => item.label);
const pairsOnly = (items) => items.filter((item) => item.kind === VALUE_KIND);
// macro_sys holds the real V32 codes; assertions on fixture codes skip them.
const nonSys = (items) => items.filter((item) => !item.label.startsWith('MACRO_SYS/'));

// Family keys are group + rank: sort as VS Code does.
function ordered(items) {
    return [...items].sort((a, b) => (a.group + a.sortText < b.group + b.sortText ? -1 : 1));
}

function range(line, start, end) {
    return { start: { line, character: start }, end: { line, character: end } };
}

async function runMain(temp) {
    const workspace = makeWorkspace(path.join(temp, 'main'));
    const macroXml = path.join(workspace.complibs, 'macro_sys', 'd_MACRO_SYS.xml');
    const session = LspSession.launch(sessionOptions(workspace));
    try {
        await session.initialize();
        await session.waitQuiet();
        const probe = new Probe(session, workspace.probeUri);

        // Off (default): the library list, unchanged.
        const off = await probe.complete('new_macro(|');
        assert(off.length > 0 && off.every((item) => item.kind === FOLDER_KIND), 'off: library items');
        assert(fs.existsSync(macroXml), 'off: the library listing exported macro_sys synchronously');
        fs.rmSync(macroXml);

        setCodeFirst(session, true);

        // First code-first request: libraries with XML now, macro_sys later.
        const first = await probe.complete('new_macro(|');
        assert(first.length > 0 && first.every((item) => item.kind === VALUE_KIND), 'on: pair items only');
        assert(!first.some((item) => item.label.startsWith('MACRO_SYS/')), 'on: macro_sys not listed before its export');
        const all = labels(ordered(first));
        const codeKeys = all.map((label) => label.slice(label.indexOf('/') + 1).toLowerCase());
        assertEqual(codeKeys, [...codeKeys].sort(), 'on: ordered by code');
        const bath = all.indexOf('CODES_LIB/BATHZONE');
        assert(bath >= 0 && all[bath + 1] === 'DUP_LIB/BATHZONE', 'on: same code from two libraries side by side');
        assert(!all.includes('CODES_LIB/'), 'on: empty code skipped');
        assert(all.includes('MIXED_CASE/FS1') && all.includes('UPPER_EXT/UX1'), 'on: fallback and uppercase-extension libraries');
        for (const item of first) {
            assertEqual(item.textEdit.range, range(0, 10, 10), `on: ${item.label} edit range`);
            assert(item.filterText === undefined, `on: ${item.label} no filterText when nothing typed`);
            assert(item.group === '0', `on: ${item.label} group 0 when nothing typed`);
        }
        const bathItem = first.find((item) => item.label === 'DUP_LIB/BATHZONE');
        assertEqual(bathItem.textEdit.newText, '"DUP_LIB", "BATHZONE"', 'on: inserts both arguments');

        // The background pass exports macro_sys; a later request lists it.
        const exported = await probe.completeUntil(
            'new_macro(|',
            (items) => items.some((item) => item.label.startsWith('MACRO_SYS/')),
            'background export'
        );
        assert(fs.existsSync(macroXml), 'background export wrote d_MACRO_SYS.xml');
        assert(exported.length > first.length, 'macro_sys codes added');
        assert(!fs.existsSync(path.join(workspace.complibs, 'broken_lib', 'd_BROKEN.xml')), 'broken_lib failed, no XML');
        assert(!exported.some((item) => item.label.startsWith('BROKEN_LIB/')), 'broken_lib not listed');

        // Matching: substring on the whole label, code matches first.
        const zone = nonSys(await probe.complete('M = new_macro(zone|'));
        assertEqual(labels(ordered(zone)), ['CODES_LIB/BATHZONE', 'DUP_LIB/BATHZONE'], 'code substring');
        assert(zone.every((item) => item.filterText === 'zone'), 'filterText is the typed text');
        assertEqual(zone[0].textEdit.range, range(0, 14, 18), 'unquoted: edit covers the typed word');

        const dupOrdered = ordered(nonSys(await probe.complete('new_macro(dup|')));
        assertEqual(labels(dupOrdered), ['DUP_LIB/DUPONLY', 'DUP_LIB/BATHZONE'], 'code match ranks first');
        assertEqual(dupOrdered.map((item) => item.group), ['0', '1'], 'library-only match is group 1');

        // `/` splits library and code.
        const split = await probe.complete('new_macro("dup/bath|")');
        assertEqual(labels(split), ['DUP_LIB/BATHZONE'], 'split match');
        assertEqual(split[0].filterText, '"dup/bath', 'split: filterText from the edit start');
        assertEqual(split[0].textEdit.range, range(0, 10, 20), 'auto-closed quote: edit covers through the closing quote');
        const libOnly = await probe.complete('new_macro(mixed/|');
        assertEqual(labels(libOnly).sort(), ['MIXED_CASE/FS1', 'MIXED_CASE/FS2'], 'library with empty code part');
        const none = await probe.complete('new_macro(qqnosuchqq|');
        assertEqual(none, [], 'no match adds nothing');

        // An existing 2nd argument is replaced; later arguments stay.
        const replace = await probe.complete('M = new_macro("bath|", "OLD", 5)');
        assertEqual(replace[0].textEdit.range, range(0, 14, 27), 'edit covers both existing arguments');
        const empty2nd = await probe.complete('new_macro(bath|, , 5)');
        assertEqual(empty2nd[0].textEdit.range, range(0, 10, 16), 'empty 2nd argument: edit stops at the next one');
        const before = await probe.complete('new_macro(|"codes_lib")');
        assertEqual(before[0].textEdit.range, range(0, 10, 21), 'cursor before a closed 1st argument replaces it');

        // A closed quoted library stays a code slot.
        const codeSlot = await probe.complete('new_macro("codes_lib", |');
        assert(codeSlot.length > 0 && codeSlot.every((item) => item.kind === VALUE_KIND && !item.label.includes('/')), 'closed library: plain codes');
        assertEqual(codeSlot[0].insertText, '"BATHZONE"', 'closed library: code insertText');

        // A comma earlier on the line no longer hides the slot.
        const comma = nonSys(await probe.complete('x = max(1, 2); new_macro(zone|'));
        assertEqual(labels(ordered(comma)), ['CODES_LIB/BATHZONE', 'DUP_LIB/BATHZONE'], 'earlier comma');

        // Other commands.
        assertEqual(nonSys(await probe.complete('J = new_djoint(zone|')).length, 2, 'new_djoint');
        assertEqual(labels(nonSys(await probe.complete('S = new_sheet(mixed|'))).sort(), ['MIXED_CASE/FS1', 'MIXED_CASE/FS2'], 'new_sheet');
        assertEqual(await probe.complete('new_piece(zone|'), [], 'not new_piece');

        // Off again, live.
        setCodeFirst(session, false);
        const offAgain = await probe.complete('new_macro(|');
        assert(offAgain.length > 0 && offAgain.every((item) => item.kind === FOLDER_KIND), 'off again: library items');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

async function runPlaceholder(temp) {
    const workspace = makeExportOnlyWorkspace(path.join(temp, 'placeholder'));
    const session = LspSession.launch(sessionOptions(workspace));
    try {
        await session.initialize();
        await session.waitQuiet();
        setCodeFirst(session, true);
        const probe = new Probe(session, workspace.probeUri);
        const first = await probe.complete('new_macro("ma|');
        assertEqual(first.length, 1, 'placeholder only');
        const [item] = first;
        assertEqual(item.label, 'Exporting 1 macro library…', 'placeholder label');
        assertEqual(item.kind, TEXT_KIND, 'placeholder kind');
        assertEqual(item.textEdit, { range: range(0, 10, 13), newText: '"ma' }, 'placeholder edit leaves the line as is');
        assertEqual(item.filterText, '"ma', 'placeholder filterText');
        const after = await probe.completeUntil(
            'new_macro("ma|',
            (items) => pairsOnly(items).length > 0,
            'placeholder export'
        );
        assert(after.every((item) => item.kind === VALUE_KIND && item.label.startsWith('MACRO_SYS/')), 'pairs replace the placeholder');
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-macro-code-first-'));
    try {
        await runMain(temp);
        await runPlaceholder(temp);
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 54-macro-code-first-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
