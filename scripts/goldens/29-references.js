'use strict';

// References goldens: test-fixtures/references/TESTING.md.
const fs = require('fs');
const path = require('path');
const { pathToFileURL, fileURLToPath } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const REPO_ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(REPO_ROOT, 'test-fixtures', 'references');

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

function fixturePath(relative) {
    return path.join(FIXTURE, ...relative.split('/'));
}

function fixture(relative) {
    const filePath = fixturePath(relative);
    const text = fs.readFileSync(filePath, 'utf8');
    return { filePath, uri: pathToFileURL(filePath).href, text, lines: text.split(/\r?\n/) };
}

function lineIndex(lines, text) {
    const i = lines.indexOf(text);
    if (i < 0) {
        throw new Error(`missing fixture line ${JSON.stringify(text)}`);
    }
    return i;
}

// Character offset of the `occurrence`-th `word` on `lineText`, plus `offset`.
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

function references(session, uri, line, character, includeDeclaration = true) {
    return session.request('textDocument/references', {
        textDocument: { uri },
        position: { line, character },
        context: { includeDeclaration },
    });
}

// "<fixture-relative path>:<line>:<start>-<end>" in result order.
function summarize(result) {
    assert(Array.isArray(result), `expected Location[], got ${JSON.stringify(result)}`);
    return result.map((location) => {
        assertEqual(location.range.start.line, location.range.end.line, 'single-line range');
        const rel = path.relative(FIXTURE, fileURLToPath(location.uri)).split(path.sep).join('/');
        return `${rel}:${location.range.start.line}:${location.range.start.character}-${location.range.end.character}`;
    });
}

async function runMain(session) {
    const main = fixture('custom/main.prg');
    const inc = fixture('custom/lib/inc.prg');
    session.didOpen({ uri: main.uri, languageId: 'vertexadt', version: 1, text: main.text });

    const resultText = 'Result = Alpha + LibVar + SysVar + Shared + Nowhere';
    const result = lineIndex(main.lines, resultText);
    const on = (word, includeDeclaration = true) =>
        references(session, main.uri, result, wordAt(resultText, word), includeDeclaration);

    // This buffer (case-insensitive, `;` comments and #EDIT_FORM# kept, `!`
    // lines / `;!` / quotes / `Alpha$` / `Alpha2` skipped), then every
    // ##included_from in source order: inc (once), sys, late (after the
    // cursor). Not `# include`, `call`, `##INCLUDED_FROM`, or an unresolved one.
    const alphaMain = [
        'custom/main.prg:1:0-5',
        'custom/main.prg:3:7-12',
        'custom/main.prg:3:15-20',
        'custom/main.prg:3:23-28',
        'custom/main.prg:9:8-13',
        'custom/main.prg:10:17-22',
        'custom/main.prg:11:13-18',
        'custom/main.prg:22:9-14',
        'custom/main.prg:23:3-8',
        'custom/main.prg:25:12-17',
    ];
    const alphaLinked = ['custom/lib/inc.prg:3:0-5', 'system/syslib/sys.prg:2:0-5', 'custom/lib/late.prg:0:0-5'];
    assertEqual(summarize(await on('Alpha')), [...alphaMain, ...alphaLinked], 'Alpha');
    assertEqual(summarize(await on('Alpha', false)), [...alphaMain.slice(1), ...alphaLinked], 'Alpha without declaration');

    // Missing `context` defaults includeDeclaration to true.
    const noContext = await session.request('textDocument/references', {
        textDocument: { uri: main.uri },
        position: { line: result, character: wordAt(resultText, 'Alpha') },
    });
    assertEqual(summarize(noContext), [...alphaMain, ...alphaLinked], 'missing context');

    // `Alpha$ ` is word `Alpha` (the `$` drops).
    const dollarText = 'x = Alpha$ + Alpha2 + MyAlpha + Alpha_b';
    assertEqual(
        summarize(await references(session, main.uri, lineIndex(main.lines, dollarText), wordAt(dollarText, 'Alpha$'))),
        [...alphaMain, ...alphaLinked],
        'Alpha$ searches Alpha'
    );

    // Definition file comes right after this buffer (Map insertion order).
    assertEqual(
        summarize(await on('Shared')),
        ['custom/main.prg:22:35-41', 'system/syslib/sys.prg:1:0-6', 'custom/lib/inc.prg:2:0-6'],
        'Shared: definition file second'
    );
    assertEqual(
        summarize(await on('Shared', false)),
        ['custom/main.prg:22:35-41', 'custom/lib/inc.prg:2:0-6'],
        'Shared without declaration drops the sys.prg span only'
    );

    // Only the one locator span drops; late.prg's own LibVar stays.
    assertEqual(
        summarize(await on('LibVar', false)),
        ['custom/main.prg:22:17-23', 'custom/lib/late.prg:1:0-6'],
        'LibVar without declaration'
    );
    // No locator: nothing is filtered.
    assertEqual(summarize(await on('Nowhere', false)), ['custom/main.prg:22:44-51'], 'Nowhere has no locator');

    // Hyphenated word vs prefix hits.
    const fooText = 'y = foo + foo-bar + foo$';
    const fooLine = lineIndex(main.lines, fooText);
    const fooBarLine = lineIndex(main.lines, 'foo-bar = 2');
    assertEqual(
        summarize(await references(session, main.uri, fooLine, wordAt(fooText, 'foo'))),
        [`custom/main.prg:${fooBarLine}:0-3`, `custom/main.prg:${fooLine}:4-7`, `custom/main.prg:${fooLine}:10-13`],
        'foo: prefix hits inside foo-bar, not foo$'
    );
    assertEqual(
        summarize(await references(session, main.uri, fooLine, wordAt(fooText, 'foo-bar', 1, 5))),
        [`custom/main.prg:${fooBarLine}:0-7`, `custom/main.prg:${fooLine}:10-17`],
        'foo-bar is one word'
    );

    // `Name$ ` drops to `Name` and skips `Name$` sites; `Name$x` keeps `$`.
    const qText = 'q = Name$ + Name';
    const q = lineIndex(main.lines, qText);
    assertEqual(summarize(await references(session, main.uri, q, wordAt(qText, 'Name$'))), [`custom/main.prg:${q}:12-16`], 'Name$ searches Name');
    const vText = 'v = Name$x + name$X';
    const v = lineIndex(main.lines, vText);
    assertEqual(
        summarize(await references(session, main.uri, v, wordAt(vText, 'Name$x'))),
        [`custom/main.prg:${v}:4-9`, `custom/main.prg:${v}:13-18`],
        'Name$x searches Name$'
    );

    // Empty results.
    const ifLine = lineIndex(main.lines, 'if Alpha > 0 then');
    assertEqual(await references(session, main.uri, ifLine, 0), [], 'if keyword');
    const msgText = 'msg$ = "Alpha" + Alpha';
    assertEqual(await references(session, main.uri, lineIndex(main.lines, msgText), wordAt(msgText, '"Alpha"', 1, 1)), [], 'word inside quotes');
    assertEqual(await references(session, main.uri, result, wordAt(resultText, ' + ', 1, 1)), [], 'no word under cursor');

    // Buffer-wins for the ##included_from file.
    session.didOpen({ uri: inc.uri, languageId: 'vertexadt', version: 1, text: '! edited\r\n\r\nAlpha = 5\r\n' });
    assertEqual(
        summarize(await on('Alpha')),
        [...alphaMain, 'custom/lib/inc.prg:2:0-5', 'system/syslib/sys.prg:2:0-5', 'custom/lib/late.prg:0:0-5'],
        'inc.prg from open buffer'
    );
    session.didClose(inc.uri);
    assertEqual(summarize(await on('Alpha')), [...alphaMain, ...alphaLinked], 'inc.prg from disk after close');

    // Non-vertexadt and unopened documents are empty.
    const plainUri = pathToFileURL(fixturePath('custom/main.txt')).href;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: main.text });
    assertEqual(await references(session, plainUri, result, wordAt(resultText, 'Alpha')), [], 'plaintext');
    session.didClose(plainUri);
    const missingUri = pathToFileURL(fixturePath('custom/not_open.prg')).href;
    assertEqual(await references(session, missingUri, 0, 0), [], 'unopened document');
}

async function runDump(session) {
    const dump = fixture('custom/dump.prg');
    session.didOpen({ uri: dump.uri, languageId: 'vertexadt', version: 1, text: dump.text });

    const countText = 'Count = Total    <2>';
    const count = lineIndex(dump.lines, countText);
    const totals = ['custom/dump.prg:1:0-5', 'custom/dump.prg:2:0-5', 'custom/dump.prg:2:8-13', `custom/dump.prg:${count}:8-13`];
    assertEqual(summarize(await references(session, dump.uri, count, wordAt(countText, 'Total'))), totals, 'dump: same regex scan');
    assertEqual(
        summarize(await references(session, dump.uri, count, wordAt(countText, 'Total'), false)),
        totals.filter((site) => site !== 'custom/dump.prg:2:0-5'),
        'dump: most-recent assignment is the locator'
    );

    // Dump parse misses hyphens and the walk finds nothing above: no filter.
    const hyphenText = 'Count = foo-bar    <1>';
    const hyphen = lineIndex(dump.lines, hyphenText);
    assertEqual(
        summarize(await references(session, dump.uri, hyphen, wordAt(hyphenText, 'foo-bar'), false)),
        [`custom/dump.prg:${hyphen}:8-15`, `custom/dump.prg:${lineIndex(dump.lines, 'foo-bar = 1    <1>')}:0-7`],
        'dump: hyphenated name has no locator'
    );
}

async function run() {
    const rootUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(REPO_ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'references' }],
    });
    try {
        await session.initialize();
        assertEqual(session.initializeResult.capabilities.referencesProvider, true, 'referencesProvider');
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
        console.log('29-references goldens passed');
    }).catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run };
