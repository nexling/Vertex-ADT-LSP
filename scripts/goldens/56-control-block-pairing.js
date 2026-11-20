'use strict';

/**
 * Native goldens for control-block pairing (docs/adr/0010): if/else/endif and
 * loop/endloop hover sections and textDocument/documentHighlight, including
 * the textual word-highlight fallback. Native only: the Node server is frozen
 * and has no document highlight provider.
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

function hoverText(result) {
    return result && result.contents ? result.contents.value : null;
}

function ranges(highlights) {
    return (highlights || [])
        .map((h) => `${h.range.start.line}:${h.range.start.character}-${h.range.end.character}`)
        .join(' ');
}

const TEXT = [
    'if a = 1 ;! first', //    0
    '  and b = 2 then', //     1
    '  loop 3', //             2
    '    total = total + 1', // 3
    '  endloop', //            4
    'else', //                 5
    '  total = 0', //          6
    'endif', //                7
    'endif', //                8 stray
].join('\r\n');

async function main() {
    const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-control-block-pairing.prg')).href;
    const session = LspSession.launch();
    const highlight = (line, character) =>
        session.request('textDocument/documentHighlight', {
            textDocument: { uri },
            position: { line, character },
        });
    try {
        const init = await session.initialize();
        assert(init.capabilities.documentHighlightProvider, 'documentHighlightProvider is advertised');
        session.didOpen({ uri, text: TEXT });
        await session.waitQuiet();

        assertEqual(ranges(await highlight(7, 2)), '0:0-2 5:0-4 7:0-5', 'cursor on endif');
        assertEqual(ranges(await highlight(0, 0)), '0:0-2 5:0-4 7:0-5', 'cursor on if');
        assertEqual(ranges(await highlight(4, 9)), '2:2-6 4:2-9', 'cursor right after endloop');
        assertEqual(ranges(await highlight(8, 0)), '8:0-5', 'stray endif highlights only itself');
        assertEqual(ranges(await highlight(3, 5)), '3:4-9 3:12-17 6:2-7', 'word fallback for a variable');

        const endifHover = hoverText(await session.hover(uri, 7, 1));
        assert(
            endifHover.startsWith('**Closes `if` (line 1):**\n```vertexadt\nif a = 1\nand b = 2 then\n```\n\n---\n'),
            `endif hover shows the two-line header: ${endifHover}`
        );
        assert(endifHover.includes("The 'endif' keyword is used to end an 'if' statement block."),
            'endif hover keeps the catalog text');
        assert(hoverText(await session.hover(uri, 0, 0)).startsWith('Closed by `endif` at line 8 (`else` at line 6)'),
            'if hover names the closer and else');
        assert(hoverText(await session.hover(uri, 5, 0)).includes('Closed by `endif` at line 8'), 'else hover');
        assert(hoverText(await session.hover(uri, 8, 0)).startsWith('No matching `if`'), 'stray endif hover');
        assert(hoverText(await session.hover(uri, 4, 3)).startsWith('**Closes `loop` (line 3):**'), 'endloop hover');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 56-control-block-pairing passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
