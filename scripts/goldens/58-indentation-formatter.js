'use strict';

/**
 * Native goldens for the indentation formatter (docs/adr/0013):
 * textDocument/formatting and rangeFormatting, the refusal warning, and the
 * vertexadt.format.enable switch. Native only: the Node server is frozen and
 * has no formatter.
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
    assert(actual === expected, `${message}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function edits(result) {
    return (result || [])
        .map((e) => `${e.range.start.line}:${e.range.start.character}-${e.range.end.character}=${JSON.stringify(e.newText)}`)
        .join(' ');
}

const TEXT = [
    'loop 3', //                   0
    'if a or \\', //               1
    'b then ;! header', //         2
    '\tx = 1', //                  3
    '   ', //                      4
    '      else', //               5
    '  dbcreate(1, \\', //         6
    '           2)', //            7
    'endif', //                    8
    '  endloop', //                9
].join('\r\n');

const OPTIONS = { tabSize: 2, insertSpaces: true };

async function main() {
    const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-indentation-formatter.prg')).href;
    const broken = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-indentation-formatter-broken.prg')).href;
    const session = LspSession.launch();
    const format = (target, options = OPTIONS) =>
        session.request('textDocument/formatting', { textDocument: { uri: target }, options });
    try {
        const init = await session.initialize();
        assert(init.capabilities.documentFormattingProvider, 'documentFormattingProvider is advertised');
        assert(init.capabilities.documentRangeFormattingProvider, 'documentRangeFormattingProvider is advertised');
        session.didOpen({ uri, text: TEXT });
        session.didOpen({ uri: broken, text: ['if a then', '  x = 1', 'endif', 'endif'].join('\n') });
        await session.waitQuiet();

        assertEqual(
            edits(await format(uri)),
            '1:0-0="  " 2:0-0="      " 3:0-1="    " 4:0-3="" 5:0-6="  " 6:0-2="    " 7:0-11="             " ' +
                '8:0-0="  " 9:0-2=""',
            'whole document'
        );
        assertEqual(
            edits(await format(uri, { tabSize: 4, insertSpaces: false })),
            '1:0-0="    " 2:0-0="            " 3:0-1="        " 4:0-3="" 5:0-6="    " 6:0-2="        " ' +
                '7:0-11="                 " 8:0-0="    " 9:0-2=""',
            'tabSize 4, spaces even when insertSpaces is false'
        );
        assertEqual(
            edits(await session.request('textDocument/rangeFormatting', {
                textDocument: { uri },
                range: { start: { line: 3, character: 0 }, end: { line: 5, character: 0 } },
                options: OPTIONS,
            })),
            '3:0-1="    " 4:0-3=""',
            'range ending at column 0 excludes that line'
        );

        const before = session._notifications.length;
        assertEqual(await format(broken), null, 'unbalanced document: no edits');
        await session.waitQuiet();
        const warning = session._notifications
            .slice(before)
            .find((m) => m.method === 'window/showMessage');
        assert(warning, 'refusal shows a message');
        assertEqual(warning.params.type, 2, 'refusal message is a warning');
        assertEqual(
            warning.params.message,
            'Vertex ADT: not formatted: unmatched `endif` at line 4.',
            'refusal message text'
        );

        session.notify('workspace/didChangeConfiguration', {
            settings: { vertexadt: { format: { enable: false } } },
        });
        await session.waitQuiet();
        assertEqual(await format(uri), null, 'vertexadt.format.enable false: no edits');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 58-indentation-formatter passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
