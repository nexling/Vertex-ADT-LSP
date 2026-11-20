'use strict';

// Definition goldens: test-fixtures/definition/TESTING.md.
const fs = require('fs');
const path = require('path');
const { pathToFileURL, fileURLToPath } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const REPO_ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(REPO_ROOT, 'test-fixtures', 'definition');

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

function splitLines(text) {
    return text.split(/\r?\n/);
}

function fixture(relative) {
    const filePath = path.join(FIXTURE, ...relative.split('/'));
    const text = fs.readFileSync(filePath, 'utf8');
    return { filePath, uri: pathToFileURL(filePath).href, text, lines: splitLines(text) };
}

function lineIndex(lines, text) {
    const i = lines.indexOf(text);
    if (i < 0) {
        throw new Error(`missing fixture line ${JSON.stringify(text)}`);
    }
    return i;
}

// Character offset of the `occurrence`-th whole `word` on `lineText`, plus `offset`.
function wordAt(lineText, word, occurrence = 1, offset = 0) {
    let from = 0;
    for (let n = 1; ; n++) {
        const at = lineText.indexOf(word, from);
        if (at < 0) {
            throw new Error(`missing ${JSON.stringify(word)} #${occurrence} on ${JSON.stringify(lineText)}`);
        }
        if (n === occurrence) {
            return at + offset;
        }
        from = at + word.length;
    }
}

function definition(session, uri, line, character) {
    return session.request('textDocument/definition', {
        textDocument: { uri },
        position: { line, character },
    });
}

function samePath(uriA, uriB) {
    return path.normalize(fileURLToPath(uriA)).toLowerCase()
        === path.normalize(fileURLToPath(uriB)).toLowerCase();
}

function assertLocation(result, expected, label) {
    assert(result && !Array.isArray(result), `${label}: expected a single Location, got ${JSON.stringify(result)}`);
    assert(samePath(result.uri, expected.uri), `${label}: uri ${result.uri} vs ${expected.uri}`);
    assertEqual(result.range, {
        start: { line: expected.line, character: expected.character },
        end: { line: expected.line, character: expected.character + expected.length },
    }, `${label} range`);
}

async function runMain(session) {
    const main = fixture('custom/main.prg');
    const lib = fixture('custom/lib/included_lib.prg');
    const sys = fixture('system/syslib/sys_only.prg');
    session.didOpen({ uri: main.uri, languageId: 'vertexadt', version: 1, text: main.text });

    const useText = 'Result = Alpha + Beta + Gamma + Delta + Hidden + foo-bar + Name$ + LibVar + SysVar + Shared + LastLineVar + UpperVar';
    const use = lineIndex(main.lines, useText);
    const at = (word, occurrence = 1, offset = 0) => definition(session, main.uri, use, wordAt(useText, word, occurrence, offset));
    const local = (line, character, length) => ({ uri: main.uri, line, character, length });

    // Nearest prior declaration in this buffer.
    assertLocation(await at('Alpha'), local(lineIndex(main.lines, 'Alpha = 1'), 0, 5), 'Alpha');
    assertLocation(await at('Beta'), local(lineIndex(main.lines, 'set Beta'), 4, 4), 'set Beta column');
    assertLocation(await at('Gamma'), local(lineIndex(main.lines, 'Gamma(3) = 5'), 0, 5), 'Gamma(3) =');
    assertEqual(await at('Delta'), null, 'trimmed ! line is skipped');
    assertEqual(await at('Hidden'), null, ';! comment is stripped');

    // Hyphen is one definition word; inclusive end.
    const fooBar = local(lineIndex(main.lines, 'foo-bar = 4'), 0, 7);
    assertLocation(await at('foo-bar', 1, 0), fooBar, 'foo-bar on f');
    assertLocation(await at('foo-bar', 1, 4), fooBar, 'foo-bar on b');
    assertLocation(await at('foo-bar', 1, 7), fooBar, 'foo-bar inclusive end');

    // `Name$ ` is definition word `Name`; `Name$ =` is not a declaration of `Name`.
    assertEqual(await at('Name$'), null, 'Name$ = is not a target for Name');

    // One hop of ##included_from, last include first, Custom then System.
    assertLocation(await at('LibVar'), { uri: lib.uri, line: lineIndex(lib.lines, 'LibVar = 10'), character: 0, length: 6 }, 'LibVar via included_lib');
    assertLocation(await at('SysVar'), { uri: sys.uri, line: lineIndex(sys.lines, 'SysVar = 20'), character: 0, length: 6 }, 'SysVar via System tree');
    assertLocation(await at('Shared'), { uri: sys.uri, line: lineIndex(sys.lines, 'Shared = 2'), character: 0, length: 6 }, 'Shared: last include first');
    assertEqual(await at('LastLineVar'), null, 'declaration on the included file last line is skipped');
    assertEqual(await at('UpperVar'), null, '##INCLUDED_FROM is not a site');

    // Current line is never the target; cursor line search runs above it.
    const reassign = lineIndex(main.lines, 'Alpha = Alpha + 1');
    assertLocation(await definition(session, main.uri, reassign, 0), local(lineIndex(main.lines, 'Alpha = 1'), 0, 5), 'LHS Alpha skips its own line');
    const self = lineIndex(main.lines, 'Self = Self');
    assertEqual(await definition(session, main.uri, self, wordAt('Self = Self', 'Self', 2)), null, 'Self = Self on its own line');

    // Quotes, keyword and command filter (exact case).
    const msgLine = lineIndex(main.lines, 'msg$ = "Alpha"');
    assertEqual(await definition(session, main.uri, msgLine, wordAt('msg$ = "Alpha"', 'Alpha')), null, 'word inside quotes');
    const ifLine = lineIndex(main.lines, 'if Alpha > 0 then');
    assertEqual(await definition(session, main.uri, ifLine, 0), null, 'if keyword');
    const otherText = 'Other = Point + point + IF';
    const other = lineIndex(main.lines, otherText);
    assertLocation(await definition(session, main.uri, other, wordAt(otherText, 'Point')), local(lineIndex(main.lines, 'Point = 3'), 0, 5), 'Point passes the filter');
    assertEqual(await definition(session, main.uri, other, wordAt(otherText, 'point')), null, 'point keyword (exact case)');
    assertEqual(await definition(session, main.uri, other, wordAt(otherText, 'IF')), null, 'IF passes the filter but has no declaration');
    assertEqual(await definition(session, main.uri, other, wordAt(otherText, ' + ', 1, 1)), null, 'no word under cursor');

    // Buffer-wins over disk for the ##included_from file.
    const edited = '! edited\r\n\r\n\r\nLibVar = 10\r\nEnd\r\n';
    session.didOpen({ uri: lib.uri, languageId: 'vertexadt', version: 1, text: edited });
    assertLocation(await at('LibVar'), { uri: lib.uri, line: 3, character: 0, length: 6 }, 'LibVar from open buffer');
    session.didClose(lib.uri);
    assertLocation(await at('LibVar'), { uri: lib.uri, line: lineIndex(lib.lines, 'LibVar = 10'), character: 0, length: 6 }, 'LibVar from disk after close');

    // Non-vertexadt is null.
    const plainUri = pathToFileURL(path.join(FIXTURE, 'custom', 'main.txt')).href;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: main.text });
    assertEqual(await definition(session, plainUri, use, wordAt(useText, 'Alpha')), null, 'plaintext');
    session.didClose(plainUri);

    // Missing document is null.
    const missingUri = pathToFileURL(path.join(FIXTURE, 'custom', 'not_open.prg')).href;
    assertEqual(await definition(session, missingUri, 0, 0), null, 'unopened document');
}

async function runDump(session) {
    const dump = fixture('custom/dump.prg');
    session.didOpen({ uri: dump.uri, languageId: 'vertexadt', version: 1, text: dump.text });
    const local = (line, character, length) => ({ uri: dump.uri, line, character, length });

    const countText = 'Count = Total + NSP    <5>';
    const count = lineIndex(dump.lines, countText);
    assertLocation(
        await definition(session, dump.uri, count, wordAt(countText, 'Total')),
        local(lineIndex(dump.lines, 'Total = Total + 1    <2>'), 0, 5),
        'dump: most-recent assignment'
    );
    assertLocation(
        await definition(session, dump.uri, count, wordAt(countText, 'NSP')),
        local(lineIndex(dump.lines, 'vec_sub(NSP) SidPceSP,WP    <-3300,254,2259.5>'), 8, 3),
        'dump: paren output jumps'
    );
    const lastTotal = lineIndex(dump.lines, 'Total = 9    <9>');
    assertLocation(
        await definition(session, dump.uri, lastTotal, 0),
        local(lastTotal, 0, 5),
        'dump: cursor line is inclusive'
    );
    const hyphenText = 'Count = foo-bar    <1>';
    assertLocation(
        await definition(session, dump.uri, lineIndex(dump.lines, hyphenText), wordAt(hyphenText, 'foo-bar')),
        local(lineIndex(dump.lines, 'foo-bar = 1    <1>'), 0, 7),
        'dump: miss falls through to the declaration walk'
    );
}

async function run() {
    const rootUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(REPO_ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'definition' }],
    });
    try {
        await session.initialize();
        assertEqual(session.initializeResult.capabilities.definitionProvider, true, 'definitionProvider');
        await runMain(session);
        await runDump(session);
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    run().then(() => {
        console.log('28-definition goldens passed');
    }).catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run };
