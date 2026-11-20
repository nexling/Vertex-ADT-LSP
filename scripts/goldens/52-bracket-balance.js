'use strict';

/**
 * Native-only goldens for unbalanced parentheses and square brackets
 * (docs/adr/0006-native-bracket-balance-check.md). No Node oracle: the Node
 * server is frozen and misses most of these.
 *
 * Pulls test-fixtures/bracket-balance/bracket_balance.prg and checks every
 * bracket diagnostic (range, message, shape), that the crossed-pair line gets
 * no extra command-syntax error, and that fixing the file clears them.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const CODE = 'vertexadt/bracket-balance';
const SOURCE = 'Syntax exception - ADT Plugin';

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

function bracketItems(report) {
    return ((report && report.items) || []).filter((item) => item.code === CODE);
}

async function main() {
    const session = LspSession.launch();
    try {
        await session.initialize();
        const file = path.join(ROOT, 'test-fixtures', 'bracket-balance', 'bracket_balance.prg');
        const uri = pathToFileURL(file).href;
        const text = fs.readFileSync(file, 'utf8');
        session.didOpen({ uri, text });

        const report = await session.diagnostic(uri);
        const items = bracketItems(report);
        assertEqual(
            items.map((item) => [item.range.start.line, item.range.start.character, item.range.end.character, item.message]),
            [
                [1, 9, 69, "Mismatching parentheses for 'call'"],
                [4, 2, 6, 'Mismatching parentheses'],
                [5, 3, 4, 'Mismatching parentheses'],
                [6, 15, 19, "Mismatching parentheses for 'f'"],
                [7, 7, 9, "Mismatching brackets for 'layer'"],
                [8, 7, 10, "Expected ']' but found ')'"],
                [11, 3, 12, 'Mismatching parentheses'],
            ],
            'bracket diagnostics'
        );
        for (const item of items) {
            assertEqual([item.severity, item.source], [1, SOURCE], 'diagnostic shape');
        }
        const syntaxOnBracketLines = report.items.filter((item) => item.source === SOURCE && item.code !== CODE
            && items.some((b) => b.range.start.line === item.range.start.line));
        assertEqual(syntaxOnBracketLines.map((item) => item.message), [], 'no second syntax error on a bracket line');

        const fixed = text
            .replace('Panel_Label$\n', 'Panel_Label$)\n')
            .replace('Panel_Label$\r\n', 'Panel_Label$)\r\n')
            .replace('x=(a+b', 'x=(a+b)')
            .replace('y=b)+1', 'y=b+1')
            .replace('f(a,b', 'f(a,b))')
            .replace('layer[a', 'layer[a]')
            .replace('abs(a[1)]', 'abs(a[1])')
            .replace('if (a=1 then', 'if (a=1) then');
        session.didChange({ uri, version: 2, contentChanges: [{ text: fixed }] });
        assertEqual(bracketItems(await session.diagnostic(uri)), [], 'clean after fixing every line');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 52-bracket-balance passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
