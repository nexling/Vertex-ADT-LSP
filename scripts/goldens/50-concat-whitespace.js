'use strict';

/**
 * Native-only goldens for whitespace around a concatenation `&`
 * (docs/adr/0005-concat-ampersand-no-whitespace.md). No Node oracle: the Node
 * server is frozen and accepts spaced `&`.
 *
 * Pull diagnostics, the per-`&` QuickFix, the file-wide SourceFixAll, and a
 * clean re-pull after applying it.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const CODE = 'vertexadt/concat-whitespace';
const MESSAGE = "Remove spaces around '&': string concatenation must not contain whitespace";

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

function concatItems(report) {
    return ((report && report.items) || []).filter((item) => item.code === CODE);
}

function spans(items) {
    return items.map((item) => [
        item.range.start.line, item.range.start.character, item.range.end.character,
    ]);
}

function codeAction(session, uri, diagnostics, only) {
    const context = { diagnostics };
    if (only) {
        context.only = only;
    }
    return session.request('textDocument/codeAction', {
        textDocument: { uri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context,
    });
}

// Single-line edits, applied in the order given (the server sorts bottom-up).
function applyEdits(text, uri, action) {
    const lines = text.split('\n');
    for (const { range, newText } of action.edit.changes[uri]) {
        const line = lines[range.start.line];
        lines[range.start.line] = line.slice(0, range.start.character) + newText
            + line.slice(range.end.character);
    }
    return lines.join('\n');
}

const TEXT = [
    'x$= date$&" "&time$',
    'y$= date$ &" " &time$',
    'debug a$ & b$',
    'if a=1 && b=2 then',
    'endif',
    'z$= "a & b"&c$ ! d$ & e$',
    '! w$= a$ & b$',
    'v$= a$ &\\',
    '    &b$',
    '',
].join('\r\n');

async function main() {
    const session = LspSession.launch();
    try {
        await session.initialize();
        const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', 'concat-whitespace-probe.prg')).href;
        session.didOpen({ uri, text: TEXT });

        const items = concatItems(await session.diagnostic(uri));
        assertEqual(spans(items), [[1, 9, 11], [1, 14, 16], [2, 8, 11], [7, 6, 8]], 'diagnostic spans');
        for (const item of items) {
            assertEqual([item.severity, item.source, item.message], [1, 'ADT-Syntax', MESSAGE], 'diagnostic shape');
        }

        const quick = await codeAction(session, uri, [items[0]], ['quickfix']);
        assertEqual(quick.map((a) => [a.title, a.kind, a.isPreferred]), [["Remove spaces around '&'", 'quickfix', true]], 'quickfix');
        assertEqual(applyEdits(TEXT, uri, quick[0]).split('\r\n')[1], 'y$= date$&" " &time$', 'quickfix applied');

        const fixAll = (await codeAction(session, uri, [], ['source.fixAll']))
            .filter((a) => a.kind === 'source.fixAll');
        assertEqual(fixAll.map((a) => a.title), ["Remove spaces around '&' in file (4)"], 'fixAll title');
        const fixed = applyEdits(TEXT, uri, fixAll[0]);
        assertEqual(fixed.split('\r\n').slice(1, 3), ['y$= date$&" "&time$', 'debug a$&b$'], 'fixAll applied');

        session.didChange({ uri, version: 2, contentChanges: [{ text: fixed }] });
        assertEqual(concatItems(await session.diagnostic(uri)), [], 'clean after fixAll');
        assertEqual(await codeAction(session, uri, [], ['source.fixAll']), [], 'nothing left to fix');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 50-concat-whitespace passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
