'use strict';

/**
 * Parity goldens for debug-filename completion.
 * Assertions from test-fixtures/debug-filename-completion/TESTING.md
 * (mirrors the Node host debug-filename provider and
 * getEnclosingSectionName). Local-only: no listing fixtures.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'debug-filename-completion');
const INVOKED = 1;
const TYPED = 2;
const FILE_KIND = 17;
const DETAIL = 'Debug output file for current document';

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

function familyItems(result) {
    assert(result && Array.isArray(result.items), 'vertexadt completion is a CompletionList');
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        'keeps the cheap keyword-variable list'
    );
    return ranking.familyItems(result, 'debugFilename', 'debug completion').filter(
        (item) => typeof item.detail === 'string' && item.detail.startsWith(DETAIL)
    );
}

class Probe {
    constructor(session, fileName) {
        this.session = session;
        this.uri = pathToFileURL(path.join(FIXTURE, fileName)).href;
        this.version = 0;
    }

    async complete(text, line, character, context) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({
                uri: this.uri,
                version: this.version,
                contentChanges: [{ text }],
            });
        }
        const params = { textDocument: { uri: this.uri }, position: { line, character } };
        if (context) {
            params.context = context;
        }
        return this.session.request('textDocument/completion', params);
    }

    async family(text, line, character, context) {
        return familyItems(await this.complete(text, line, character, context));
    }

    // `|` marks the cursor (removed before sending). UTF-16 column.
    async at(textWithCursor, context) {
        const lines = textWithCursor.split('\n');
        const line = lines.findIndex((row) => row.includes('|'));
        assert(line >= 0, 'probe text has a | cursor');
        const character = lines[line].indexOf('|');
        lines[line] = lines[line].replace('|', '');
        return this.family(lines.join('\n'), line, character, context);
    }
}

function expectItem(items, label, section, name) {
    assertEqual(items.map((item) => item.label), [label], `${name} labels`);
    const item = items[0];
    assertEqual(item.kind, FILE_KIND, `${name} kind File`);
    assertEqual(item.insertText, label, `${name} insertText is the label`);
    assertEqual(
        item.detail,
        section ? `${DETAIL} and section ${section}` : DETAIL,
        `${name} detail`
    );
    assertEqual(item.sortText, '0', `${name} sortText`);
    assert(item.textEdit === undefined, `${name} has no textEdit`);
    assert(item.filterText === undefined, `${name} has no filterText`);
    assert(item.data === undefined, `${name} has no data`);
    assert(item.documentation === undefined, `${name} has no documentation`);
}

function expectNone(items, name) {
    assertEqual(items.map((item) => item.label), [], `${name} adds no debug-filename item`);
}

async function run(session) {
    const init = await session.initialize();
    assertEqual(
        init.capabilities.completionProvider.triggerCharacters,
        [' ', ',', '(', '/', '"', ')'],
        'completion trigger characters stay ticket 09 union'
    );

    const wall = new Probe(session, 'wall.prg');
    const plain = './_wall.prg.dbg';
    const sec = (name) => `./_wall.prg_${name}.dbg`;

    // Site: /^\s*debug\s+$/ on the UTF-16 prefix line[0..cursor).
    expectItem(await wall.at('debug |'), plain, null, 'debug space');
    expectItem(await wall.at('debug   |'), plain, null, 'debug many spaces');
    expectItem(await wall.at('debug\t|'), plain, null, 'debug tab');
    expectItem(await wall.at('    debug |'), plain, null, 'indented');
    expectItem(await wall.at(' debug　|'), plain, null, 'JS \\s NBSP / ideographic space');
    expectItem(await wall.at('debug |foo'), plain, null, 'prefix end, not line end');
    expectItem(await wall.family('debug ', 0, 99), plain, null, 'character past line end clamps');
    expectNone(await wall.at('debug foo|'), 'after filename text');
    expectNone(await wall.at('debug|'), 'no space');
    expectNone(await wall.at('DEBUG |'), 'uppercase');
    expectNone(await wall.at('Debug |'), 'mixed case');
    expectNone(await wall.at('debugger |'), 'debugger');
    expectNone(await wall.at('x = debug |'), 'mid-line');
    expectNone(await wall.at('debug ./|'), 'after ./');
    expectNone(await wall.at('debug "./|'), 'after quote');
    expectNone(await wall.at('debug(|'), 'paren');
    expectNone(await wall.at('\u{1F600}debug |'), 'astral char before debug');

    // Enclosing section: getEnclosingSectionName, not parseFileSections.
    expectItem(await wall.at('#Macros#\ndebug |\n#MACROS#'), sec('Macros'), 'Macros', 'previous spelling, case-insensitive pair');
    expectItem(await wall.at('#A# x #B#\n\ndebug |\n\n#b#'), sec('B'), 'B', 'previous line last match, skips blank lines');
    expectItem(await wall.at('#A##B#\ndebug |\n#B#'), sec('B'), 'B', 'non-overlapping /g matches');
    expectItem(await wall.at('#A#\ndebug |\n#A# #C#'), sec('A'), 'A', 'next line first match');
    expectItem(await wall.at('#A#\ndebug |\n#C# #A#'), plain, null, 'next first match differs: no section');
    expectItem(await wall.at('debug |\n#A#'), plain, null, 'no previous marker: no section');
    expectItem(await wall.at('#A#\ndebug |'), plain, null, 'no next marker: no section');
    expectItem(await wall.at('#OUTER#\n#INNER#\ndebug |\n#OUTER#'), plain, null, 'unclosed inner then outer closer: no section');
    expectItem(await wall.at('#my-sec#\ndebug |\n#my-sec#'), plain, null, 'hyphen names do not match: no section');
    expectItem(await wall.at('#X#\ndebug |#Y#\n#X#'), sec('X'), 'X', 'current-line marker skipped');
    expectItem(await wall.at('#Y#\ndebug |#Y#'), plain, null, 'current-line marker is not a next marker: no section');
    expectItem(await wall.at('  !#1A# ;\ndebug |\nfoo #1a# bar'), sec('1A'), '1A', 'indented bang digit-start mid-line markers');
    expectItem(await wall.at('#EDIT_FORM#\ndebug |\n#EDIT_FORM#'), sec('EDIT_FORM'), 'EDIT_FORM', '#EDIT_FORM# pairs');
    expectItem(await wall.at('#S#\r\ndebug |\r\n#s#'), sec('S'), 'S', 'CRLF');
    expectItem(await wall.at('#S#\n;FILE x SECTION y\ndebug |\n#S#'), sec('S'), 'S', 'markerless lines between are skipped');

    // Basename keeps the extension; fsPath is decoded.
    const spaced = new Probe(session, 'my wall.v.prg');
    expectItem(await spaced.at('debug |'), './_my wall.v.prg.dbg', null, 'decoded basename with spaces and dots');

    // Trigger kind ignored; keyword stays visible; resolve leaves it unchanged.
    expectItem(await wall.at('debug |', { triggerKind: TYPED, triggerCharacter: ' ' }), plain, null, 'typed space');
    expectItem(await wall.at('debug |', { triggerKind: INVOKED }), plain, null, 'invoked');
    const full = await wall.complete('debug ', 0, 6);
    assert(
        full.items.some((item) => item.label === 'debug' && item.data !== undefined),
        'catalog debug keyword stays visible'
    );
    const resolved = await session.completionResolve(familyItems(full)[0]);
    assertEqual(resolved.documentation, undefined, 'resolve does not fill the debug-filename item');
    assertEqual(resolved.label, plain, 'resolve keeps label');

    const plainUri = pathToFileURL(path.join(FIXTURE, 'plain.txt')).href;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: 'debug ' });
    const plainResult = await session.request('textDocument/completion', {
        textDocument: { uri: plainUri },
        position: { line: 0, character: 6 },
    });
    assertEqual(plainResult, null, 'non-vertexadt is null');

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'debug-filename-completion' }],
    });
    try {
        await run(session);
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 34-debug-filename-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
