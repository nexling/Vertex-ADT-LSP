#!/usr/bin/env node
'use strict';

// Inferred ##included_from parents (docs/adr/0015-native-inferred-included-from.md).
// Fixture: test-fixtures/inferred-included-from (see its TESTING.md).

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'inferred-included-from');
const CUSTOM = path.join(FIXTURE, 'custom');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    if (actualJson !== expectedJson) {
        throw new Error(`${message}: expected ${expectedJson}, got ${actualJson}`);
    }
}

function delay(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function uriOf(relative) {
    return pathToFileURL(path.join(CUSTOM, relative)).href;
}

function open(session, relative) {
    const uri = uriOf(relative);
    const text = fs.readFileSync(path.join(CUSTOM, relative), 'utf8');
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    return { uri, text };
}

function undeclared(items) {
    return (items || [])
        .map((item) => String(item.message))
        .map((message) => {
            const match = message.match(/Variable (\S+) used before declaration/);
            return match ? match[1] : null;
        })
        .filter(Boolean)
        .sort();
}

async function codeLenses(session, uri) {
    return (await session.request('textDocument/codeLens', { textDocument: { uri } })) || [];
}

// The startup index is debounced; wait until the child's lens shows up.
async function waitForLens(session, uri) {
    for (let attempt = 0; attempt < 50; attempt += 1) {
        const lenses = await codeLenses(session, uri);
        const lens = lenses.find((item) => item.command && item.command.command === 'vertexadt.openIncludedFromParents');
        if (lens) {
            return lens;
        }
        await delay(100);
    }
    throw new Error(`no inferred-parents CodeLens for ${uri}`);
}

async function run() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'inferred-included-from' }],
    });
    await session.initialize();

    const child = open(session, 'macros/child.prg');
    const lens = await waitForLens(session, child.uri);
    await session.waitQuiet();

    // 1. CodeLens: three parents (p5's plain call does not count).
    assertEqual(lens.command.title, 'Included from: progs/p1.prg +2', 'child lens title');
    assertEqual(
        lens.command.arguments[0].map((row) => row.displayPath),
        ['progs/p1.prg', 'progs/p2.prg', 'progs/p3.prg'],
        'child lens parents'
    );
    assertEqual(lens.range, { start: { line: 0, character: 0 }, end: { line: 0, character: 1 } }, 'lens range');

    // 2. Intersection + include point + per-section scope. A, B, J1 come from
    // every reaching parent; C is declared after p2's include; LATE after
    // both includes; J2 only in p3, but p1/p2 include the whole file (and
    // so JOINT) without it.
    const childPull = await session.diagnostic(child.uri);
    assertEqual(undeclared(childPull.items), ['C', 'J2', 'LATE'], 'child undeclared names');

    // 3. Definition: one location per parent declaring A above its include.
    const definition = await session.request('textDocument/definition', {
        textDocument: { uri: child.uri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(definition), `definition array: ${JSON.stringify(definition)}`);
    assertEqual(
        definition.map((location) => path.basename(decodeURIComponent(location.uri))).sort(),
        ['p1.prg', 'p2.prg'],
        'definition parents'
    );

    // 4. Hover comes from the first parent in path order.
    const hover = await session.hover(child.uri, 0, 4);
    const hoverText = hover && hover.contents && (hover.contents.value || hover.contents);
    assert(String(hoverText).includes('A from p1'), `hover comment: ${JSON.stringify(hoverText)}`);

    // 5. Pin source actions: one per parent plus "Pin all".
    const actions = await session.request('textDocument/codeAction', {
        textDocument: { uri: child.uri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [], only: ['source'] },
    });
    assertEqual(
        actions.map((action) => action.title),
        [
            'Pin ##included_from progs/p1.prg',
            'Pin ##included_from progs/p2.prg',
            'Pin ##included_from progs/p3.prg',
            'Pin all 3 inferred parents as ##included_from',
        ],
        'pin action titles'
    );
    assertEqual(
        actions[0].edit.changes[child.uri][0].newText,
        '##included_from progs/p1.prg\n',
        'pin edit text'
    );
    const quickFixes = await session.request('textDocument/codeAction', {
        textDocument: { uri: child.uri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [] },
    });
    assertEqual(quickFixes.length, 0, 'no pin actions without only:[source]');

    // 6. A written line stops at the parent's include point: W2 is set after it.
    const written = open(session, 'macros/written.prg');
    const writtenPull = await session.diagnostic(written.uri);
    assertEqual(undeclared(writtenPull.items), ['W2'], 'written cut-off');
    const writtenLenses = await codeLenses(session, written.uri);
    assert(
        !writtenLenses.some((item) => item.command && item.command.command === 'vertexadt.openIncludedFromParents'),
        'no inferred lens under a whole-file written line'
    );

    // 7. A written line that resolves nowhere warns.
    const broken = open(session, 'macros/broken.prg');
    const brokenPull = await session.diagnostic(broken.uri);
    const notFound = brokenPull.items.filter((item) => item.code === 'vertexadt/included-from-not-found');
    assertEqual(notFound.length, 1, 'file-not-found count');
    assertEqual(notFound[0].severity, 2, 'file-not-found is a warning');
    assertEqual(notFound[0].message, '##included_from file not found: progs/nope.prg', 'file-not-found message');
    assertEqual(notFound[0].range.start, { line: 0, character: 16 }, 'file-not-found start');

    // 8. Over the cap (11 parents): nothing inferred, the lens says so.
    const capped = open(session, 'macros/capchild.prg');
    const cappedLens = await waitForLens(session, capped.uri);
    assertEqual(cappedLens.command.title, 'Included from: 11 parents, too many to infer, pin one', 'cap lens');
    const cappedPull = await session.diagnostic(capped.uri);
    assertEqual(undeclared(cappedPull.items), ['CAPVAR'], 'cap: nothing inferred');

    // 9. Setting off: no inference, no lens.
    session.didChangeConfiguration({ inferIncludedFrom: false });
    await session.waitQuiet(600);
    const offPull = await session.diagnostic(child.uri);
    assertEqual(undeclared(offPull.items), ['A', 'B', 'C', 'J1', 'J2', 'LATE'], 'setting off: nothing inferred');
    const offLenses = await codeLenses(session, child.uri);
    assert(
        !offLenses.some((item) => item.command && item.command.command === 'vertexadt.openIncludedFromParents'),
        'setting off: no lens'
    );

    await session.shutdownAndExit();
}

module.exports = { run };

if (require.main === module) {
    run().then(
        () => console.log('60-inferred-included-from passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
