'use strict';

/**
 * Native-only goldens for literal substring index checks and substring
 * assignment (docs/adr/0006-substring-index-validation.md). No Node oracle:
 * the Node server is frozen and does not check substrings.
 *
 * Pull diagnostics, the start-index QuickFix, and a clean re-pull after it.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const SOURCE = 'Syntax exception - ADT Plugin';
const START = 'vertexadt/substring-start-index';
const END = 'vertexadt/substring-end-index';
const EMPTY = 'vertexadt/substring-empty-range';
const ASSIGN = 'vertexadt/substring-assignment';
const CODES = new Set([START, END, EMPTY, ASSIGN]);

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

function substringItems(report) {
    return ((report && report.items) || []).filter((item) => CODES.has(item.code));
}

function shape(items) {
    return items.map((item) => [
        item.code, item.severity, item.range.start.line,
        item.range.start.character, item.range.end.character,
    ]);
}

const TEXT = [
    'panellabel$ = filename$[0,dotindex-1]',
    'ok$ = filename$[1,dotindex-1]',
    'tail$ = a$[P+11,0]',
    'last$ = a$[len(a$),len(a$)]',
    'ch$ = a$[I,I]',
    'neg$ = a$[1,-1]',
    'empty$ = a$[5,3]',
    'a$[I,I]="T"',
    'if a$[1,1]="T" then',
    'endif',
    'x$ = "b$[0,1]" ! c$[0,1]',
    '',
].join('\r\n');

async function main() {
    const session = LspSession.launch();
    try {
        await session.initialize();
        const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', 'substring-index-probe.prg')).href;
        session.didOpen({ uri, text: TEXT });

        const items = substringItems(await session.diagnostic(uri));
        assertEqual(shape(items), [
            [START, 1, 0, 24, 25],
            [END, 2, 5, 12, 14],
            [EMPTY, 2, 6, 12, 15],
            [ASSIGN, 1, 7, 0, 7],
        ], 'diagnostics');
        for (const item of items) {
            assertEqual(item.source, SOURCE, 'source');
        }
        assertEqual(
            items[0].message,
            'Substring start index must be 1 or higher (strings are 1-indexed); got 0.',
            'start message');

        const actions = await session.request('textDocument/codeAction', {
            textDocument: { uri },
            range: items[0].range,
            context: { diagnostics: [items[0]], only: ['quickfix'] },
        });
        const fixes = actions.filter((a) => a.title === 'Use 1-based start index');
        assertEqual(fixes.map((a) => [a.kind, a.isPreferred]), [['quickfix', true]], 'quickfix');
        const [edit] = fixes[0].edit.changes[uri];
        const lines = TEXT.split('\r\n');
        const line = lines[edit.range.start.line];
        lines[edit.range.start.line] = line.slice(0, edit.range.start.character) + edit.newText
            + line.slice(edit.range.end.character);
        assertEqual(lines[0], 'panellabel$ = filename$[1,dotindex-1]', 'quickfix applied');

        session.didChange({ uri, version: 2, contentChanges: [{ text: lines.join('\r\n') }] });
        const after = substringItems(await session.diagnostic(uri));
        assertEqual(after.filter((item) => item.code === START), [], 'start error gone after fix');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 51-substring-index passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
