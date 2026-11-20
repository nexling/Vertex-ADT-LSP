'use strict';

/**
 * Native-only goldens for findstr* value expressions
 * (docs/adr/0011-findstr-value-expressions.md). No Node oracle: the Node
 * server is frozen and flags `x = findstrri(...)-1`, lacks findstrr, and types
 * findstr* results as boolean.
 *
 * Pull diagnostics with strict mode off and on.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');

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

const TEXT = [
    'panellabel$ = "AB-CD"',
    'b$ = "-"',
    'end = findstrri(panellabel$,"-")-1',
    'end = findstrri(panellabel$,"-")+1',
    'p = findstr(panellabel$,b$) - len(b$)',
    'q = findstri(panellabel$,b$)*2',
    'r = findstrr(panellabel$,b$)-1',
    'findstrri(panellabel$,b$)-1',
    's = findstr(panellabel$,b$) b$',
    't = findstrr(panellabel$)',
    'ok = findstri(panellabel$,"walls@") or findstri(panellabel$,"deck@")',
    'oktest = !(findstri(panellabel$,"-M-") and ok)',
    'oktest = (findstri(panellabel$,"-M-"))',
    'Val = findstr(panellabel$,b$);',
    'abs(end)',
    'abs(r)',
    'debug end p q r s t ok oktest Val',
    '',
].join('\r\n');

// [line, message] for findstr*/abs diagnostics, in report order.
function shape(report) {
    return ((report && report.items) || [])
        .filter((item) => /findstr|abs/.test(item.message))
        .map((item) => [item.range.start.line, item.message]);
}

const FLAGGED = [
    [7, 'findstrri accepts at most 0 parameters after parentheses'],
    [8, 'findstr accepts at most 0 parameters after parentheses'],
    [9, 'findstrr requires at least 2 parameters inside parentheses'],
];

async function main() {
    const session = LspSession.launch();
    try {
        await session.initialize();
        const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', 'findstr-value-expression-probe.prg')).href;
        session.didOpen({ uri, text: TEXT });
        assertEqual(shape(await session.diagnostic(uri)), FLAGGED, 'strict off');

        session.didChangeConfiguration({ vertexadt: { strictMode: { enabled: true } } });
        session.didChange({ uri, version: 2, contentChanges: [{ text: TEXT }] });
        assertEqual(shape(await session.diagnostic(uri)), FLAGGED, 'strict on: findstr* results are values');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 57-findstr-value-expression passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
