'use strict';

// Document-link goldens: test-fixtures/document-links/TESTING.md.
const fs = require('fs');
const path = require('path');
const { pathToFileURL, fileURLToPath } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const REPO_ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(REPO_ROOT, 'test-fixtures', 'document-links');

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

function captureRange(lineText, rawPath) {
    const start = lineText.indexOf(rawPath);
    if (start < 0) {
        throw new Error(`missing capture ${JSON.stringify(rawPath)} on ${JSON.stringify(lineText)}`);
    }
    return {
        start: { line: null, character: start },
        end: { line: null, character: start + rawPath.length },
    };
}

function onLine(range, line) {
    return {
        start: { line, character: range.start.character },
        end: { line, character: range.end.character },
    };
}

function targetPath(uri) {
    const withoutHash = String(uri).split('#')[0];
    return path.normalize(fileURLToPath(withoutHash)).toLowerCase();
}

function targetFragment(uri) {
    const hash = String(uri).includes('#') ? String(uri).slice(String(uri).indexOf('#') + 1) : '';
    return hash;
}

function samePath(uriA, uriB) {
    return targetPath(uriA) === targetPath(uriB);
}

function documentLink(session, uri) {
    return session.request('textDocument/documentLink', {
        textDocument: { uri },
    });
}

function assertLink(link, expected, label) {
    assert(link, `${label}: missing link`);
    assertEqual(link.range, expected.range, `${label} range`);
    assert(samePath(link.target, expected.uri), `${label}: uri ${link.target} vs ${expected.uri}`);
    assertEqual(targetFragment(link.target), expected.fragment || '', `${label} fragment`);
    const fsPath = fileURLToPath(String(link.target).split('#')[0]);
    const section = expected.section ? ` #${expected.section}#` : '';
    assertEqual(link.tooltip, `Open ${fsPath}${section}`, `${label} tooltip`);
}

async function runCaller(session, languageId) {
    const caller = fixture('caller.prg');
    const target = fixture('target.prg');
    const sectioned = fixture('sectioned.prg');
    const dollar = fixture('macros/from_dollar.prg');
    const dbg = fixture('_caller.dbg');
    const uri = languageId === 'vertexadt'
        ? caller.uri
        : pathToFileURL(path.join(FIXTURE, `caller.${languageId}.txt`)).href;
    session.didOpen({ uri, languageId, version: 1, text: caller.text });
    const links = await documentLink(session, uri);
    assert(Array.isArray(links), `${languageId} caller: array`);

    const expectPath = (lineText, rawPath, targetUri, fragment, count, label, section) => {
        const line = lineIndex(caller.lines, lineText);
        const range = onLine(captureRange(lineText, rawPath), line);
        const matches = links.filter((link) => JSON.stringify(link.range) === JSON.stringify(range));
        assertEqual(matches.length, count, `${label} count`);
        for (const link of matches) {
            assertLink(link, { range, uri: targetUri, fragment, section }, label);
        }
    };

    expectPath('# include "target.prg"', '"target.prg"', target.uri, '', 1, 'quoted include');
    expectPath('#include target.prg', 'target.prg', target.uri, '', 1, '#include no space');
    const upper = lineIndex(caller.lines, '# INCLUDE target.prg');
    assertEqual(
        links.filter((link) => link.range.start.line === upper).length,
        0,
        '# INCLUDE is not a site'
    );
    expectPath('# include sectioned.prg JOINT', 'sectioned.prg', sectioned.uri, 'L2', 1, 'JOINT fragment', 'JOINT');
    expectPath('  # include sectioned.prg SECTION', 'sectioned.prg', sectioned.uri, 'L5', 1, 'SECTION ident', 'SECTION');
    expectPath('x = 1; # include target.prg JOINT', 'target.prg', target.uri, '', 1, 'mid-line include no fragment');
    expectPath('##included_from target.prg', 'target.prg', target.uri, '', 1, '##included_from no fragment');
    expectPath('! # include target.prg', 'target.prg', target.uri, '', 1, 'bang include');
    expectPath('Ret$=call("target.prg")', '"target.prg"', target.uri, '', 2, 'assignment+call duplicate');
    // Named-section suffixes on call / quoted include (native only, ADR 0006).
    expectPath('Ret$=call("sectioned.prg JOINT",x$)', '"sectioned.prg JOINT"', sectioned.uri, 'L2', 2,
        'quoted call( suffix', 'JOINT');
    expectPath('call sectioned.prg JOINT', 'sectioned.prg', sectioned.uri, 'L2', 1, 'call space suffix', 'JOINT');
    expectPath('# include "sectioned.prg JOINT"', '"sectioned.prg JOINT"', sectioned.uri, 'L2', 1,
        'quoted include suffix', 'JOINT');
    expectPath('Ret$=call("sectioned.prg NOSUCH",x$)', '"sectioned.prg NOSUCH"', sectioned.uri, '', 2,
        'missing section opens file');

    for (const text of ['CALL("target.prg")', 'recall("target.prg")', 'call_virtual("target.prg")']) {
        const line = lineIndex(caller.lines, text);
        assertEqual(
            links.filter((link) => link.range.start.line === line).length,
            0,
            `${text} is not a site`
        );
    }
    expectPath('call target.prg', 'target.prg', target.uri, '', 1, 'space-form call');
    const spaced = lineIndex(caller.lines, 'call ("target.prg")');
    assertEqual(
        links.filter((link) => link.range.start.line === spaced).length,
        0,
        'call ( captures parens and does not resolve'
    );
    expectPath(
        '# include "$DIR_MACROS/from_dollar.prg"',
        '"$DIR_MACROS/from_dollar.prg"',
        dollar.uri,
        '',
        1,
        '$DIR_MACROS overlay'
    );
    for (const text of [
        '# include "$DIR_EXPORT/nope.prg"',
        '# include missing_not_a_file.prg',
        'debug macros/from_dollar.prg',
        'x debug "./_caller.dbg"',
        'debug "./../outside.prg"',
    ]) {
        const line = lineIndex(caller.lines, text);
        assertEqual(
            links.filter((link) => link.range.start.line === line).length,
            0,
            `${text} produces no link`
        );
    }
    expectPath('debug "./_caller.dbg"', '"./_caller.dbg"', dbg.uri, '', 1, 'debug ./');
    expectPath("  debug './_caller.dbg'", "'./_caller.dbg'", dbg.uri, '', 1, 'indented debug ./');

    session.didClose(uri);
    return links;
}

async function runDump(session, languageId) {
    const dump = fixture('dump.prg');
    const target = fixture('target.prg');
    const sectioned = fixture('sectioned.prg');
    const dbg = fixture('_caller.dbg');
    const uri = languageId === 'vertexadt'
        ? dump.uri
        : pathToFileURL(path.join(FIXTURE, `dump.${languageId}.txt`)).href;
    session.didOpen({ uri, languageId, version: 1, text: dump.text });
    const links = await documentLink(session, uri);
    assertEqual(links.length, 3, `${languageId} dump link count`);

    const includeLine = lineIndex(dump.lines, '# include target.prg');
    assertLink(links[0], {
        range: onLine(captureRange('# include target.prg', 'target.prg'), includeLine),
        uri: target.uri,
        fragment: '',
    }, `${languageId} dump include first`);

    const headerLine = dump.lines[0];
    const headerRange = onLine(captureRange(headerLine, 'sectioned.prg'), 0);
    assertLink(links[1], {
        range: headerRange,
        uri: sectioned.uri,
        fragment: 'L2',
    }, `${languageId} dump header second`);

    const debugLine = lineIndex(dump.lines, 'debug "./_caller.dbg"');
    assertLink(links[2], {
        range: onLine(captureRange('debug "./_caller.dbg"', '"./_caller.dbg"'), debugLine),
        uri: dbg.uri,
        fragment: '',
    }, `${languageId} dump debug third`);

    session.didClose(uri);
}

async function runMissingHeaders(session) {
    const missing = fixture('dump-missing.prg');
    session.didOpen({ uri: missing.uri, languageId: 'vertexadt', version: 1, text: missing.text });
    const missingLinks = await documentLink(session, missing.uri);
    assertEqual(missingLinks.length, 1, 'missing non-$ header still links');
    const missingPath = 'C:\\vertex-adt-no-such-file.prg';
    assertLink(missingLinks[0], {
        range: onLine(captureRange(missing.lines[0], missingPath), 0),
        uri: pathToFileURL(missingPath).href,
        fragment: '',
    }, 'missing header FILE');
    session.didClose(missing.uri);

    const dollar = fixture('dump-dollar-miss.prg');
    session.didOpen({ uri: dollar.uri, languageId: 'vertexadt', version: 1, text: dollar.text });
    const dollarLinks = await documentLink(session, dollar.uri);
    assertEqual(dollarLinks, [], '$ header miss is no link');
    session.didClose(dollar.uri);
}

async function runEmpty(session) {
    const uri = pathToFileURL(path.join(FIXTURE, 'empty.prg')).href;
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text: '' });
    const links = await documentLink(session, uri);
    assertEqual(links, [], 'empty vertexadt');
    session.didClose(uri);

    const missingUri = pathToFileURL(path.join(FIXTURE, 'not-open.prg')).href;
    assertEqual(await documentLink(session, missingUri), [], 'unopened document');
}

async function withSession(workspaceName, fn) {
    const rootUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(REPO_ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: workspaceName }],
    });
    try {
        await session.initialize();
        await fn(session);
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

async function run() {
    await withSession('user-document-links', async (session) => {
        assertEqual(
            session.initializeResult.capabilities.documentLinkProvider,
            { resolveProvider: false },
            'documentLinkProvider.resolveProvider'
        );
        await runEmpty(session);
        await runCaller(session, 'vertexadt');
        await runCaller(session, 'plaintext');
        await runDump(session, 'vertexadt');
        await runDump(session, 'log');
        await runMissingHeaders(session);
    });

    await withSession('document-links', async (session) => {
        const caller = fixture('caller.prg');
        session.didOpen({
            uri: caller.uri,
            languageId: 'vertexadt',
            version: 1,
            text: caller.text,
        });
        const links = await documentLink(session, caller.uri);
        const debugLines = [
            lineIndex(caller.lines, 'debug "./_caller.dbg"'),
            lineIndex(caller.lines, "  debug './_caller.dbg'"),
        ];
        for (const line of debugLines) {
            assertEqual(
                links.filter((link) => link.range.start.line === line).length,
                0,
                `no debug-command link on line ${line} without user folder`
            );
        }
        const quoted = lineIndex(caller.lines, '# include "target.prg"');
        assert(
            links.some((link) => link.range.start.line === quoted),
            'include links remain without user folder'
        );
    });
}

if (require.main === module) {
    run().then(() => {
        console.log('31-document-links goldens passed');
    }).catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run };
