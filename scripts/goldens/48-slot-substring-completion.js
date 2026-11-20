'use strict';

/**
 * Native-only goldens for slot completion substring matching
 * (docs/adr/0002-native-slot-completion-substring-match.md). No Node oracle:
 * the Node server is frozen and does not do this.
 *
 * Form names exercise the textEdit path (quoted and unquoted slots) and the
 * per-slot listing cache. Macro libraries exercise a slot that is only a site
 * at the typed word's start (`new_macro(cor` is the `new_macro(` slot).
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const MACRO_SYS = path.join(ROOT, 'test-fixtures', 'vertex-dbase-converter', 'custom', 'complibs', 'macro_sys');
const INVOKED = 1;
const INCOMPLETE = 3;
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

// Family items in client order (the full sortText, as VS Code sorts on a tie).
function ordered(result, family, name) {
    assert(result && Array.isArray(result.items), `${name}: returns a CompletionList`);
    assert(result.items.some((item) => item.label === 'askdim'), `${name}: keeps the cheap keyword-variable list`);
    const byFullSort = result.items
        .filter((item) => ranking.isFamilyItem(item, family))
        .sort((a, b) => (a.sortText < b.sortText ? -1 : a.sortText > b.sortText ? 1 : 0))
        .map((item) => item.label);
    const items = ranking.familyItemsOf(result, family, name);
    const byLabel = Object.fromEntries(items.map((item) => [item.label, item]));
    return byFullSort.map((label) => byLabel[label]);
}

class Probe {
    constructor(session, uri) {
        this.session = session;
        this.uri = uri;
        this.version = 0;
    }

    // `|` marks the cursor on the last line; without one it is at the end.
    async complete(textWithCursor, triggerKind, timeoutMs) {
        const lines = textWithCursor.split('\n');
        const line = lines.length - 1;
        let character = lines[line].indexOf('|');
        if (character >= 0) {
            lines[line] = lines[line].replace('|', '');
        } else {
            character = lines[line].length;
        }
        const text = lines.join('\n');
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
        return this.session.request('textDocument/completion', {
            textDocument: { uri: this.uri },
            position: { line, character },
            context: { triggerKind },
        }, timeoutMs);
    }
}

function launch(root) {
    const rootUri = pathToFileURL(root).href;
    return LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: path.basename(root) }],
    });
}

async function runFormNames() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-slot-substring-'));
    try {
        const customForms = path.join(temp, 'customa', 'forms');
        fs.mkdirSync(customForms, { recursive: true });
        fs.mkdirSync(path.join(temp, 'system', 'forms'), { recursive: true });
        for (const file of ['1cornermodel.vxf', 'cornerpost.vxf', 'wall.vxf', 'Ärende.vxf']) {
            fs.writeFileSync(path.join(customForms, file), '');
        }
        const session = launch(temp);
        try {
            await session.initialize();
            await session.waitQuiet();
            const probe = new Probe(session, pathToFileURL(path.join(temp, 'customa', 'probe.prg')).href);

            // Nothing typed: every form, all in the start-of-label group, filterText untouched.
            let items = ordered(await probe.complete('dbedit(1,"|', INVOKED), 'formName', 'empty quoted slot');
            assertEqual(items.map((item) => item.label).sort(), ['1cornermodel.vxf', 'cornerpost.vxf', 'wall.vxf', 'Ärende.vxf'].sort(), 'empty slot lists all');
            assert(items.every((item) => item.group === '0'), 'empty slot: one group');
            assert(items.every((item) => item.filterText === item.label), 'empty slot: filterText is the name');

            // The reported case: a substring that is not the start of the name.
            items = ordered(await probe.complete('dbedit(1,"cornermodel|', INCOMPLETE), 'formName', 'cornermodel');
            assertEqual(items.map((item) => item.label), ['1cornermodel.vxf'], 'cornermodel finds 1cornermodel');
            assertEqual(items[0].group, '1', 'mid-label match group');
            assertEqual(items[0].filterText, 'cornermodel', 'filterText is the typed text');
            assertEqual(items[0].textEdit.range, {
                start: { line: 0, character: 10 },
                end: { line: 0, character: 21 },
            }, 'textEdit replaces the typed text');

            // Start-of-label matches first, then the rest; case-insensitive.
            items = ordered(await probe.complete('dbedit(1,"CORNER|', INCOMPLETE), 'formName', 'CORNER');
            assertEqual(items.map((item) => [item.label, item.group]), [['cornerpost.vxf', '0'], ['1cornermodel.vxf', '1']], 'prefix before substring');

            // Non-ASCII folds case too.
            items = ordered(await probe.complete('dbedit(1,"äRE|', INCOMPLETE), 'formName', 'äRE');
            assertEqual(items.map((item) => item.label), ['Ärende.vxf'], 'non-ASCII case fold');

            // No match: no family items, but the slot stays incomplete.
            const none = await probe.complete('dbedit(1,"zzz|', INCOMPLETE);
            assertEqual(ranking.familyItemsOf(none, 'formName', 'zzz'), [], 'no match adds nothing');
            assertEqual(none.isIncomplete, true, 'an active slot is incomplete even when empty');

            // Unquoted slot: found at the typed word's start, range stretched to the cursor.
            items = ordered(await probe.complete('dbedit(1,corn|', INCOMPLETE), 'formName', 'unquoted corn');
            assertEqual(items.map((item) => item.label), ['cornerpost.vxf', '1cornermodel.vxf'], 'unquoted slot filters');
            assertEqual(items[0].textEdit.range, {
                start: { line: 0, character: 9 },
                end: { line: 0, character: 13 },
            }, 'unquoted range covers the typed word');
            assertEqual(items[0].filterText, 'corn', 'unquoted filterText');

            // Cache: re-requests in the same slot reuse the listing; a new request reads fresh.
            ordered(await probe.complete('dbedit(1,"|', INVOKED), 'formName', 'enter slot');
            fs.writeFileSync(path.join(customForms, 'cornerlate.vxf'), '');
            items = ordered(await probe.complete('dbedit(1,"corner|', INCOMPLETE), 'formName', 'cached');
            assert(!items.some((item) => item.label === 'cornerlate.vxf'), 'a re-request reuses the slot listing');
            items = ordered(await probe.complete('dbedit(1,"corner|', INVOKED), 'formName', 'fresh');
            assert(items.some((item) => item.label === 'cornerlate.vxf'), 'a new request reads fresh');

            // Off any slot: untouched cheap list, complete.
            const plain = await probe.complete('x = 1\ny|', INVOKED);
            assertEqual(plain.isIncomplete, false, 'no slot: complete');
            ranking.assertUnranked(plain.items, 'no slot');

            await session.shutdownAndExit();
        } catch (err) {
            session.kill();
            throw err;
        }
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
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

async function runMacroLibraries() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-slot-substring-macro-'));
    try {
        const complibs = path.join(temp, 'customa', 'complibs');
        copyDir(MACRO_SYS, path.join(complibs, 'macro_sys'));
        const system = path.join(temp, 'system');
        fs.mkdirSync(path.join(system, 'complibs'), { recursive: true });
        // The macro-library golden's layout: System and Custom as two folders.
        const session = LspSession.launch({
            rootUri: pathToFileURL(temp).href,
            extensionUri: pathToFileURL(ROOT).href,
            workspaceFolders: [
                { uri: pathToFileURL(system).href, name: 'system' },
                { uri: pathToFileURL(path.join(temp, 'customa')).href, name: 'customa' },
            ],
        });
        try {
            await session.initialize();
            await session.waitQuiet();
            const probe = new Probe(session, pathToFileURL(path.join(temp, 'customa', 'probe.prg')).href);

            let items = ordered(await probe.complete('M = new_macro(|', INVOKED, XML_TIMEOUT_MS), 'macroLibrary', 'library slot');
            assertEqual(items.map((item) => item.label), ['MACRO_SYS'], 'library list');

            // `new_macro(sys` is the `new_macro(` slot with `sys` typed.
            items = ordered(await probe.complete('M = new_macro(sys|', INCOMPLETE, XML_TIMEOUT_MS), 'macroLibrary', 'sys');
            assertEqual(items.map((item) => [item.label, item.group, item.filterText]), [['MACRO_SYS', '1', 'sys']], 'substring library');
            assertEqual(items[0].insertText, '"MACRO_SYS"', 'insertText keeps the quotes');

            items = ordered(await probe.complete('M = new_macro(zzz|', INCOMPLETE, XML_TIMEOUT_MS), 'macroLibrary', 'zzz');
            assertEqual(items, [], 'no library matches zzz');

            await session.shutdownAndExit();
        } catch (err) {
            session.kill();
            throw err;
        }
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
}

async function main() {
    await runFormNames();
    await runMacroLibraries();
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 48-slot-substring-completion passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
