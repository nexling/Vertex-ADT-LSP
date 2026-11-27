'use strict';

/**
 * Native goldens for infinite-loop checks (docs/adr/0027): a bare `loop`
 * with no exitif/break/exit/quit is a warning; an exitif whose condition
 * variables never change in the body is information. `# include` is
 * followed through the workspace, `call` silences the exitif note, and
 * vertexadt.diagnostics.warnInfiniteLoops turns both off.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'infinite-loops');
const CUSTOM = path.join(FIXTURE, 'custom');
const ROOT_URI = pathToFileURL(FIXTURE).href;
const CODES = new Set(['vertexadt/infinite-loop', 'vertexadt/exitif-never-changes']);

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const a = JSON.stringify(actual);
    const e = JSON.stringify(expected);
    assert(a === e, `${message}: expected ${e}, got ${a}`);
}

function messageOf(item) {
    return typeof item.message === 'string' ? item.message : (item.message && item.message.value) || '';
}

// "W3:0-4" for a warning, "I5:2-8" for information.
function summary(items) {
    return (items || [])
        .filter((item) => CODES.has(item.code))
        .map((item) => `${item.severity === 2 ? 'W' : 'I'}${item.range.start.line}:`
            + `${item.range.start.character}-${item.range.end.character}`)
        .join(' ');
}

const CASES = [
    {
        name: 'no way out',
        text: ['loop', '  a = a + 1', 'endloop'],
        want: 'W0:0-4',
    },
    {
        name: 'counted loop',
        text: ['loop 10', '  a = a + 1', 'endloop'],
        want: '',
    },
    {
        name: 'exitif with a ticker',
        text: ['i = 0', 'loop', '  i = i + 1', '  exitif i > 10', 'endloop'],
        want: '',
    },
    {
        name: 'nested exitif does not leave the outer loop',
        text: ['loop', '  loop', '    j = j + 1', '    exitif j > 3', '  endloop', 'endloop'],
        want: 'W0:0-4',
    },
    {
        name: 'inline if then break',
        text: ['loop', '  if a = 1 then break', 'endloop'],
        want: '',
    },
    {
        name: 'condition never changes',
        text: ['i = 0', 'loop', '  a = 1', '  exitif i > 10', 'endloop'],
        want: 'I3:2-8',
    },
    {
        name: 'out_ parameter changes the condition',
        text: ['loop', '  Sup0 = get_trs_supports(Trs,Sup)', '  exitif !Sup', 'endloop'],
        want: '',
    },
    {
        name: 'in_ parameter does not',
        text: ['loop', '  Sup0 = get_trs_supports(Trs,Sup)', '  exitif !Trs', 'endloop'],
        want: 'I2:2-8',
    },
    {
        name: 'uncataloged command may write',
        text: ['loop', '  my_own_command Obj', '  exitif !Obj', 'endloop'],
        want: '',
    },
    {
        name: 'call silences the note',
        text: ['loop', '  call "sub.prg"', '  exitif !Obj', 'endloop'],
        want: '',
    },
    {
        name: 'include with exitif',
        text: ['loop', '  # include inc_exitif.prg', 'endloop'],
        want: '',
    },
    {
        name: 'include without exitif',
        text: ['loop', '  # include inc_noexit.prg', 'endloop'],
        want: 'W0:0-4',
    },
    {
        name: 'include changes the condition',
        text: ['loop', '  # include inc_changes.prg', '  exitif !Obj', 'endloop'],
        want: '',
    },
];

async function main() {
    const session = LspSession.launch({
        rootUri: ROOT_URI,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: ROOT_URI, name: 'infinite-loops' }],
    });
    try {
        await session.initialize();
        session.didChangeConfiguration({ diagnostics: { maxNumberOfProblems: 100000 } });
        await session.waitQuiet(200);

        for (let i = 0; i < CASES.length; i += 1) {
            const c = CASES[i];
            const uri = pathToFileURL(path.join(CUSTOM, `.golden-infinite-${i}.prg`)).href;
            session.didOpen({ uri, languageId: 'vertexadt', version: 1, text: c.text.join('\r\n') + '\r\n' });
            const report = await session.diagnostic(uri);
            assertEqual(summary(report.items), c.want, c.name);
            if (i === 0) {
                const item = report.items.find((d) => d.code === 'vertexadt/infinite-loop');
                assertEqual(messageOf(item), 'Infinite loop: no exitif/break/exit inside loop ... endloop.',
                    'warning message');
                assertEqual(item.source, 'ADT-Syntax', 'source');
            }
            if (c.name === 'condition never changes') {
                const item = report.items.find((d) => d.code === 'vertexadt/exitif-never-changes');
                assertEqual(messageOf(item),
                    'exitif condition can never become true inside this loop: `i` is never changed in the loop body.',
                    'information message');
                assertEqual(item.severity, 3, 'information severity');
            }
            session.didClose(uri);
        }

        session.didChangeConfiguration({ diagnostics: { warnInfiniteLoops: false } });
        await session.waitQuiet(200);
        const offUri = pathToFileURL(path.join(CUSTOM, '.golden-infinite-off.prg')).href;
        session.didOpen({ uri: offUri, languageId: 'vertexadt', version: 1, text: 'loop\r\nendloop\r\n' });
        assertEqual(summary((await session.diagnostic(offUri)).items), '', 'setting off');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 65-infinite-loops passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
