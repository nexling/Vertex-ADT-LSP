'use strict';

/**
 * Parity goldens for form-name completion.
 * Assertions from test-fixtures/form-name-completion/TESTING.md.
 *
 * Oracle: server/out/formNameCompletion.js (analyzeFormNameCompletion +
 * collectWorkspaceFormNames) run through the client host's item mapping
 * (client/out/extension.js), with Node's custom/getFormNames folded into
 * ordinary textDocument/completion.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');
const support = require('../../server/out/formNameCompletion');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'form-name-completion');
const FILE_KIND = 17;
const INVOKED = 1;
const TYPED = 2;
const ORIGINS = new Set(['custom/forms', 'system/forms']);

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

// Host provider mapping over analyzeFormNameCompletion + custom/getFormNames.
function oracleItems(lineText, line, character, nodeOptions) {
    const analysis = support.analyzeFormNameCompletion(lineText, character);
    if (!analysis) {
        return [];
    }
    const forms = support.collectWorkspaceFormNames(nodeOptions);
    const range = {
        start: { line, character: analysis.replaceStart },
        end: { line, character: analysis.replaceEnd },
    };
    return forms.map((form) => {
        const insertText = support.formInsertText(form.name, analysis.insideQuotes);
        return {
            label: form.name,
            kind: FILE_KIND,
            detail: form.origin,
            filterText: form.name,
            sortText: form.name.toLowerCase(),
            insertText,
            textEdit: { range, newText: insertText },
        };
    });
}

// Native slot substring match (docs/adr/0002) over the frozen Node oracle:
// the site is found at the cursor, else at the typed word's start; ranges
// that end before the cursor stretch to it; items are kept when the label
// contains the typed text (range start to cursor, leading spaces/quotes
// ignored, case-insensitive) and take that text as filterText.
function wordStartOf(lineText, cursor) {
    let start = cursor;
    if (start > 1 && lineText[start - 1] === '$' && /\w/.test(lineText[start - 2])) {
        start -= 1;
    }
    while (start > 0 && /\w/.test(lineText[start - 1])) {
        start -= 1;
    }
    return start;
}

function nativeOracleItems(lineText, line, cursor, nodeOptions) {
    let items = oracleItems(lineText, line, cursor, nodeOptions);
    const wordStart = wordStartOf(lineText, cursor);
    if (items.length === 0 && wordStart < cursor) {
        items = oracleItems(lineText, line, wordStart, nodeOptions);
    }
    return items.flatMap((item) => {
        const range = item.textEdit.range;
        const end = range.end.character < cursor ? { line, character: cursor } : range.end;
        const start = range.start.character <= cursor ? range.start.character : wordStart;
        const filter = lineText.slice(start, cursor);
        const needle = filter.replace(/^[\s"']+/, '').toLowerCase();
        if (needle && !item.label.toLowerCase().includes(needle)) {
            return [];
        }
        return [{
            ...item,
            filterText: filter ? filter : item.filterText,
            textEdit: { range: { start: range.start, end }, newText: item.textEdit.newText },
        }];
    });
}

// The lowercased typed text the native filter matches labels against ('' when
// nothing is typed or no site).
function typedNeedle(lineText, cursor) {
    const wordStart = wordStartOf(lineText, cursor);
    let analysis = support.analyzeFormNameCompletion(lineText, cursor);
    if (!analysis && wordStart < cursor) {
        analysis = support.analyzeFormNameCompletion(lineText, wordStart);
    }
    if (!analysis) {
        return '';
    }
    const start = analysis.replaceStart <= cursor ? analysis.replaceStart : wordStart;
    return lineText.slice(start, cursor).replace(/^[\s"']+/, '').toLowerCase();
}

function familyItems(result, name) {
    assert(result && Array.isArray(result.items), `${name}: vertexadt completion is a CompletionList`);
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        `${name}: keeps the cheap keyword-variable list`
    );
    return ranking.familyItems(result, 'formName', name)
        .filter((item) => ORIGINS.has(item.detail))
        .map((item) => {
            for (const field of ['insertTextFormat', 'documentation', 'data', 'commitCharacters', 'command']) {
                assert(item[field] === undefined, `${name}: ${item.label} has no ${field}`);
            }
            return {
                label: item.label,
                kind: item.kind,
                detail: item.detail,
                filterText: item.filterText,
                sortText: item.sortText,
                insertText: item.insertText,
                textEdit: item.textEdit,
            };
        });
}

class Probe {
    constructor(session, uri, nodeOptions) {
        this.session = session;
        this.uri = uri;
        this.nodeOptions = nodeOptions;
        this.version = 0;
    }

    async complete(text, line, character, context) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
        const params = { textDocument: { uri: this.uri }, position: { line, character } };
        if (context) {
            params.context = context;
        }
        return this.session.request('textDocument/completion', params);
    }

    // `|` marks the cursor on the last line; without one it is at the end.
    // `character` overrides the column (to probe clamping).
    async check(textWithCursor, name, expectedLabels, { context = { triggerKind: INVOKED }, character } = {}) {
        const lines = textWithCursor.split('\n');
        const line = lines.length - 1;
        let column = lines[line].indexOf('|');
        if (column >= 0) {
            lines[line] = lines[line].replace('|', '');
        } else {
            column = lines[line].length;
        }
        const sent = typeof character === 'number' ? character : column;
        const clamped = Math.min(sent, lines[line].length);
        const result = await this.complete(lines.join('\n'), line, sent, context);
        const items = familyItems(result, name);
        assertEqual(items, nativeOracleItems(lines[line], line, clamped, this.nodeOptions), `${name}: matches the shaped Node oracle`);
        if (expectedLabels !== undefined) {
            const needle = typedNeedle(lines[line], clamped);
            assertEqual(
                items.map((item) => item.label),
                expectedLabels.filter((label) => !needle || label.toLowerCase().includes(needle)),
                `${name}: labels`
            );
        }
        return items;
    }
}

const FIXTURE_NAMES = ['alpha.vxf', 'notes.txt', 'shared.vxf', 'system_only.vxf'];

// [line with optional `|` cursor, site?]
const SITE_ROWS = [
    ['edit_obj_data(Obj,', true],
    ['ok = edit_jnt_data(conn, ', true],
    ['dbedit(1,', true],
    ['set_macro_form(Mac)', true],
    ['set_macro_form(Mac) ', true],
    ['edit_obj_data(Obj,"form', true],
    ['edit_obj_data(Obj,"form|.vxf",0)', true],
    ['edit_obj_data(Obj,"form.vxf|",0)', true],
    ['edit_obj_data(Obj,|"form.vxf",0)', true],
    ['edit_obj_data(Obj, |"form.vxf",0)', true],
    ['set_macro_form(Mac) "form', true],
    ["dbedit(1,'form", true],
    ['edit_obj_data(foo(a,b), "', true],
    ['EDIT_OBJ_DATA(Obj, "', true],
    ['Dbedit(1, "', true],
    ['! edit_obj_data(Obj, "', true],
    ['edit_obj_data(Obj, "a;|b",0)', true],
    ['edit_obj_data (Obj, "', true],
    ['\tedit_obj_data\t(Obj,\t"', true],
    ['.edit_obj_data(Obj,"', true],
    ['Ok=edit_obj_data(Obj,"', true],
    ['edit_obj_data(Obj,|)', true],
    ['edit_obj_data(, "', true],
    ['dbedit(,"form', true],
    ['set_macro_form()', true],
    ['edit_obj_data(Obj, |Form$', true],
    ['dbedit(1,"foo\\bar', true],
    ['set_macro_form(Mac) dbedit(1,"', true],
    ['dbedit(1,"a") set_macro_form(M) "', true],
    ['Å = edit_obj_data(Obj, "fo', true],
    ['😀 edit_obj_data(Obj, "fo|rm"', true],
    ['edit_obj_data(Obj', false],
    ['edit_obj_data(', false],
    ['edit_obj_data|(Obj, "', false],
    // Native: a word typed in the slot is the slot at its start (filtered).
    ['edit_obj_data(Obj, Form$', true],
    ['edit_obj_data(Obj, F|orm$', true],
    ['edit_obj_data(Obj, ascii(65)', false],
    ['edit_obj_data(Obj, "form.vxf", ', false],
    ['edit_obj_data(Obj, "form.vxf" ', false],
    ['edit_jnt_data(conn,"form.vxf")', false],
    ['edit_obj_data(Obj,"form.vxf")|', false],
    ['set_macro_form(Mac', false],
    ['set_macro_form(Mac|)', false],
    ['set_macro_form(Mac, "', false],
    ['set_macro_form(Mac) "form.vxf"', false],
    ['set_macro_form(Mac) "a", "', false],
    ['dbeditfrm 1, "', false],
    ['dbeditfrm(1, "', false],
    ['dbledit(1, "', false],
    ['xedit_obj_data(Obj, "', false],
    ['new_macro("LIB"', false],
    ['edit_obj_data(Obj, "x") ; comment', false],
    ['x = 1; edit_obj_data(Obj, "', false],
    ['dbedit 1, "', false],
    ['', false],
];

async function runFixtureRoot() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'form-name-completion' }],
    });
    try {
        const init = await session.initialize();
        assertEqual(
            init.capabilities.completionProvider.triggerCharacters,
            [' ', ',', '(', '/', '"', ')'],
            'completion trigger characters stay ticket 09 union'
        );
        await session.waitQuiet();
        const nodeOptions = { workspaceRoot: FIXTURE, customName: 'custom', workspaceFolders: [FIXTURE] };
        const probe = new Probe(session, pathToFileURL(path.join(FIXTURE, 'custom', 'probe.prg')).href, nodeOptions);

        for (const [row, site] of SITE_ROWS) {
            await probe.check(row, JSON.stringify(row), site ? FIXTURE_NAMES : []);
        }

        // Item fields: wrap-quoted insert at the cursor, Custom wins `shared.vxf`.
        let items = await probe.check('edit_obj_data(Obj,', 'item fields', FIXTURE_NAMES);
        const empty = { start: { line: 0, character: 18 }, end: { line: 0, character: 18 } };
        assertEqual(items[2], {
            label: 'shared.vxf',
            kind: FILE_KIND,
            detail: 'custom/forms',
            filterText: 'shared.vxf',
            sortText: 'shared.vxf',
            insertText: '"shared.vxf"',
            textEdit: { range: empty, newText: '"shared.vxf"' },
        }, 'empty slot item');
        assertEqual(items.map((item) => item.detail), ['custom/forms', 'custom/forms', 'custom/forms', 'system/forms'], 'origins');

        // Inside quotes: bare name replacing the quoted content, quotes kept.
        // (Typed `vx` keeps the `.vxf` names: slot substring match.)
        items = await probe.check("dbedit(1,'vx|f.form')", 'single-quoted content', FIXTURE_NAMES);
        assertEqual(items[0].filterText, 'vx', 'filterText is the typed text');
        assertEqual(items[0].textEdit, {
            range: { start: { line: 0, character: 10 }, end: { line: 0, character: 18 } },
            newText: 'alpha.vxf',
        }, 'replace quoted content');

        // Multi-line buffer, CRLF, trigger kinds, and no context.
        await probe.check('x = 1\r\ny = 2\r\nedit_obj_data(Obj, "', 'third CRLF line', FIXTURE_NAMES);
        await probe.check('edit_obj_data(Obj, "', 'typed quote', FIXTURE_NAMES, { context: { triggerKind: TYPED, triggerCharacter: '"' } });
        await probe.check('set_macro_form(Mac)', 'typed )', FIXTURE_NAMES, { context: { triggerKind: TYPED, triggerCharacter: ')' } });
        await probe.check('dbedit(1,', 'no context', FIXTURE_NAMES, { context: null });
        await probe.check('dbedit(1,', 'cursor past EOL clamps', FIXTURE_NAMES, { character: 999 });

        // Non-vertexadt: no family.
        const plainUri = pathToFileURL(path.join(FIXTURE, 'custom', 'other.txt')).href;
        session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: 'edit_obj_data(Obj, "' });
        const plain = await session.request('textDocument/completion', {
            textDocument: { uri: plainUri },
            position: { line: 0, character: 20 },
            context: { triggerKind: INVOKED },
        });
        const plainItems = plain && Array.isArray(plain.items) ? plain.items : Array.isArray(plain) ? plain : [];
        assertEqual(plainItems.filter((item) => ORIGINS.has(item.detail)), [], 'plaintext adds no form items');

        // completionItem/resolve leaves these items alone.
        items = await probe.check('dbedit(1,', 'resolve probe', FIXTURE_NAMES);
        const resolved = await session.completionResolve({ ...items[0] });
        assertEqual(
            [resolved.label, resolved.detail, resolved.insertText, resolved.textEdit],
            [items[0].label, items[0].detail, items[0].insertText, items[0].textEdit],
            'resolve keeps the item'
        );

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

// Other workspace shapes resolve the same two `forms/` directories.
async function runLayout(name, folders, nodeOptions, expected) {
    const rootUri = pathToFileURL(folders[0]).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: folders.map((folder) => ({ uri: pathToFileURL(folder).href, name: path.basename(folder) })),
    });
    try {
        await session.initialize();
        await session.waitQuiet();
        const probe = new Probe(session, pathToFileURL(path.join(folders[0], 'probe.prg')).href, nodeOptions);
        await probe.check('edit_obj_data(Obj, "', name, expected);
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

// Listing details the checked-in fixture cannot show: case-insensitive Custom
// win, kept `D_`/`.vxm`, sort order, non-ASCII names, and no listing cache.
async function runListingWorkspace() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-form-goldens-'));
    try {
        const customForms = path.join(temp, 'customa', 'forms');
        const systemForms = path.join(temp, 'system', 'forms');
        fs.mkdirSync(path.join(customForms, 'Sub'), { recursive: true });
        fs.mkdirSync(systemForms, { recursive: true });
        for (const file of ['Shared.VXF', 'D_part.vxm', '_under.vxf', '1num.vxf', 'Ärende.vxf', 'zeta.vxf', 'a-b.vxf', 'a.b.vxf', 'ab.vxf', 'Sub/deep.vxf']) {
            fs.writeFileSync(path.join(customForms, file), '');
        }
        for (const file of ['shared.vxf', 'Ångest.vxf', 'Beta.vxf', 'zeta.VXF', 'noext']) {
            fs.writeFileSync(path.join(systemForms, file), '');
        }
        const rootUri = pathToFileURL(temp).href;
        const session = LspSession.launch({
            rootUri,
            extensionUri: pathToFileURL(ROOT).href,
            workspaceFolders: [{ uri: rootUri, name: path.basename(temp) }],
        });
        try {
            await session.initialize();
            await session.waitQuiet();
            const nodeOptions = { workspaceRoot: temp, customName: 'customa', workspaceFolders: [temp] };
            const probe = new Probe(session, pathToFileURL(path.join(temp, 'customa', 'probe.prg')).href, nodeOptions);
            const before = await probe.check('dbedit(1,"', 'listing workspace');
            const byName = Object.fromEntries(before.map((item) => [item.label, item]));
            assertEqual(byName['Shared.VXF'] && byName['Shared.VXF'].detail, 'custom/forms', 'Custom wins case-insensitively, casing kept');
            assert(!byName['shared.vxf'], 'System duplicate is dropped');
            assert(byName['D_part.vxm'] && byName.noext, 'D_ and .vxm and extensionless names are kept');
            assert(!byName.Sub && !byName['deep.vxf'], 'directories and nested files are skipped');
            assertEqual(byName['Ärende.vxf'].sortText, 'ärende.vxf', 'sortText lowercases non-ASCII');

            fs.writeFileSync(path.join(systemForms, 'late.vxf'), '');
            const after = await probe.check('dbedit(1,"', 'file added after start');
            assert(after.some((item) => item.label === 'late.vxf'), 'listing is read per request');
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
    await runFixtureRoot();
    const custom = path.join(FIXTURE, 'custom');
    const system = path.join(FIXTURE, 'system');
    await runLayout(
        'custom + system folders',
        [custom, system],
        { workspaceRoot: FIXTURE, customName: 'custom', workspaceFolders: [custom, system] },
        FIXTURE_NAMES
    );
    await runLayout(
        'single Custom folder',
        [custom],
        { workspaceRoot: custom, customName: 'custom', workspaceFolders: [custom] },
        FIXTURE_NAMES
    );
    const noForms = path.join(ROOT, 'test-fixtures', 'cheap-hover-completion');
    await runLayout(
        'no forms directories',
        [noForms],
        { workspaceRoot: noForms, customName: 'custom', workspaceFolders: [noForms] },
        []
    );
    await runListingWorkspace();
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 39-form-name-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
