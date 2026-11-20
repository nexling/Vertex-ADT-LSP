#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'unclosed-resources');
const CUSTOM = path.join(FIXTURE, 'custom');
const ROOT_URI = pathToFileURL(FIXTURE).href;

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

function fixtureUri(name) {
    return pathToFileURL(path.join(CUSTOM, name)).href;
}

function fixtureText(name) {
    return fs.readFileSync(path.join(CUSTOM, name), 'utf8');
}

function unclosed(items) {
    return (items || []).filter((item) => (
        item.code === 'vertexadt/unclosed-resource'
        || item.code === 'vertexadt/unclosed-already-open'
    ));
}

function messageOf(item) {
    if (!item) {
        return '';
    }
    if (typeof item.message === 'string') {
        return item.message;
    }
    return (item.message && item.message.value) || '';
}

async function pullUnclosed(session, uri) {
    const report = await session.diagnostic(uri);
    assertEqual(report && report.kind, 'full', `pull kind for ${uri}`);
    return unclosed(report.items);
}

async function openFixture(session, name, version = 1) {
    const uri = fixtureUri(name);
    session.didOpen({
        uri,
        languageId: 'vertexadt',
        version,
        text: fixtureText(name),
    });
    return uri;
}

async function openText(session, name, text, version = 1) {
    const uri = fixtureUri(name);
    session.didOpen({
        uri,
        languageId: 'vertexadt',
        version,
        text,
    });
    return uri;
}

async function withSession(fn) {
    const session = LspSession.launch({
        rootUri: ROOT_URI,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: ROOT_URI, name: 'unclosed-resources' }],
    });
    try {
        await fn(session);
    } catch (err) {
        session.kill();
        throw err;
    }
}

async function runGoldens(session) {
    await session.initialize();

    const unclosedUri = await openFixture(session, 'unclosed_dbopen.prg');
    const unclosedItems = await pullUnclosed(session, unclosedUri);
    assertEqual(unclosedItems.length, 1, 'unclosed dbopen count');
    assert(
        messageOf(unclosedItems[0]).includes('Database 1 is opened here'),
        `unclosed dbopen message: ${messageOf(unclosedItems[0])}`
    );
    assertEqual(unclosedItems[0].severity, 2, 'unclosed dbopen warning');
    assertEqual(unclosedItems[0].code, 'vertexadt/unclosed-resource', 'unclosed dbopen code');
    assertEqual(unclosedItems[0].source, 'ADT-Syntax', 'unclosed dbopen source');
    assertEqual(unclosedItems[0].range.start.line, 0, 'unclosed dbopen line');

    const closedUri = await openFixture(session, 'closed_dbopen.prg');
    assertEqual(await pullUnclosed(session, closedUri), [], 'matched dbopen/dbclose');

    const alreadyUri = await openFixture(session, 'already_open.prg');
    const alreadyItems = await pullUnclosed(session, alreadyUri);
    assertEqual(alreadyItems.length, 1, 'ALREADY_OPEN count');
    assertEqual(alreadyItems[0].severity, 3, 'ALREADY_OPEN information');
    assertEqual(alreadyItems[0].code, 'vertexadt/unclosed-already-open', 'ALREADY_OPEN code');
    assert(
        messageOf(alreadyItems[0]).includes('ALREADY_OPEN'),
        `ALREADY_OPEN message: ${messageOf(alreadyItems[0])}`
    );

    const includeUri = await openFixture(session, 'include_opener.prg');
    const includeItems = await pullUnclosed(session, includeUri);
    assertEqual(includeItems.length, 1, 'include leftover count');
    assert(
        messageOf(includeItems[0]).includes('Included code opens database 1'),
        `include leftover message: ${messageOf(includeItems[0])}`
    );
    assertEqual(includeItems[0].severity, 2, 'include leftover warning');
    assertEqual(includeItems[0].range.start.line, 0, 'include leftover on include line');
    assert(includeItems[0].relatedInformation && includeItems[0].relatedInformation.length === 1,
        'include leftover relatedInformation');
    assert(
        messageOf(includeItems[0].relatedInformation[0]).includes('Database 1 is opened here'),
        'include leftover related points at the real open'
    );

    const ignoreUri = await openFixture(session, 'ignore_close.prg');
    assertEqual(await pullUnclosed(session, ignoreUri), [], 'IGNORE CLOSE on dbopen');

    const includeIgnoreUri = await openFixture(session, 'include_ignore_close.prg');
    assertEqual(await pullUnclosed(session, includeIgnoreUri), [], 'IGNORE CLOSE on include');

    const afterCloseUri = await openText(
        session,
        'open_after_close.prg',
        'dbopen(1) "SHEETLIB",1\ndbclose(1)\ndbopen(1) "SHEETLIB",1\n'
    );
    const afterCloseItems = await pullUnclosed(session, afterCloseUri);
    assertEqual(afterCloseItems.length, 1, 'open after close');

    const twoOpensUri = await openText(
        session,
        'two_opens.prg',
        'dbopen(1) "A",1\ndbopen(1) "A",1\ndbclose(1)\n'
    );
    assertEqual(await pullUnclosed(session, twoOpensUri), [], 'two opens then one later close');

    const fileVsDbUri = await openText(
        session,
        'file_vs_db.prg',
        'open(1) "file.txt"\ndbclose(1)\n'
    );
    const fileVsDbItems = await pullUnclosed(session, fileVsDbUri);
    assertEqual(fileVsDbItems.length, 1, 'file open not closed by dbclose');
    assert(
        messageOf(fileVsDbItems[0]).includes('File 1 is opened here'),
        `file vs db message: ${messageOf(fileVsDbItems[0])}`
    );

    const matchedFileUri = await openText(
        session,
        'matched_file.prg',
        'open(1) "file.txt"\nclose(1)\n'
    );
    assertEqual(await pullUnclosed(session, matchedFileUri), [], 'matched open/close');

    const byNameUri = await openText(
        session,
        'dbclose_by_name.prg',
        'dbopen(1) "C:/lib/PROREG",1\nok = dbclose_by_name(\'PROREG\')\n'
    );
    assertEqual(await pullUnclosed(session, byNameUri), [], 'dbclose_by_name basename');

    const dbcreateUri = await openText(session, 'dbcreate_open.prg', 'dbcreate(2) "TEMPDB",0,0\n');
    const dbcreateItems = await pullUnclosed(session, dbcreateUri);
    assertEqual(dbcreateItems.length, 1, 'dbcreate is an open');

    const includeThenCloseUri = await openText(
        session,
        'include_then_close.prg',
        '# include opener.prg\ndbclose(1)\n'
    );
    assertEqual(await pullUnclosed(session, includeThenCloseUri), [], 'include then close in parent');

    const sectionUri = await openText(
        session,
        'section_include.prg',
        '# include sectional.prg USED\n'
    );
    assertEqual(await pullUnclosed(session, sectionUri), [], 'section include ignores other section open');

    const withCommonsUri = await openText(
        session,
        'call_with_commons.prg',
        'common_dbases 1\ndbopen(1) "SHEETLIB",1\ncall("closer.prg")\n'
    );
    assertEqual(await pullUnclosed(session, withCommonsUri), [], 'callee close with common_dbases');

    const withoutCommonsUri = await openText(
        session,
        'call_without_commons.prg',
        'dbopen(1) "SHEETLIB",1\ncall("closer.prg")\n'
    );
    const withoutCommonsItems = await pullUnclosed(session, withoutCommonsUri);
    assertEqual(withoutCommonsItems.length, 1, 'callee close without common_dbases');

    const callOpenerUri = await openText(
        session,
        'call_opener.prg',
        'call("opener.prg")\n'
    );
    assertEqual(await pullUnclosed(session, callOpenerUri), [], 'no call leftover');

    const fragmentUri = await openFixture(session, 'fragment.prg');
    const fragmentItems = await pullUnclosed(session, fragmentUri);
    assertEqual(fragmentItems.length, 1, 'fragment own unmatched open');
    assert(
        messageOf(fragmentItems[0]).includes('Database 1 is opened here'),
        `fragment message: ${messageOf(fragmentItems[0])}`
    );

    await runPathAwareGoldens(session);

    session.didChange({
        uri: unclosedUri,
        version: 2,
        contentChanges: [{ text: 'dbopen(1) "SHEETLIB",1 ;!IGNORE CLOSE\n' }],
    });
    assertEqual(await pullUnclosed(session, unclosedUri), [], 'IGNORE CLOSE after edit');

    session.didChangeConfiguration({
        diagnostics: { warnUnclosedResources: false },
    });
    await session.waitQuiet(200);
    session.didChange({
        uri: alreadyUri,
        version: 2,
        contentChanges: [{ text: fixtureText('already_open.prg') }],
    });
    assertEqual(await pullUnclosed(session, alreadyUri), [], 'setting off skips the walk');

    await session.shutdownAndExit();
}

function summary(items) {
    return items
        .map((item) => ({
            line: item.range.start.line,
            message: messageOf(item),
            related: (item.relatedInformation || [])
                .map((info) => `${info.location.uri.split('/').pop()}:${info.location.range.start.line}`)
                .sort(),
        }))
        .sort((a, b) => a.line - b.line);
}

// Native-only (docs/adr/0012-native-path-aware-unclosed-resources.md): sections
// are separate roots, branches/loops/exit/break are followed, $CALLPARAM
// handles are closed up the caller chain. The frozen Node server has none of it.
async function runPathAwareGoldens(session) {
    const chainUri = await openFixture(session, 'chain_timer.prg');
    const chain = summary(await pullUnclosed(session, chainUri));
    assertEqual(chain.map((item) => item.line), [3, 6, 26], 'chain: opens and failing call line');
    for (const item of chain.slice(0, 2)) {
        assert(item.message.includes('Database dbid is opened here and not closed on every caller path'),
            `chain open message: ${item.message}`);
        assertEqual(item.related, ['chain_timer.prg:26'], 'chain open links only the failing caller');
    }
    assert(chain[2].message.includes('Database h opened by the called code is not closed on every path after this call'),
        `chain call-line message: ${chain[2].message}`);
    assertEqual(chain[2].related, ['chain_timer.prg:3', 'chain_timer.prg:6'], 'chain call line links both opens');

    const branchItems = summary(await pullUnclosed(session, await openFixture(session, 'branch_close.prg')));
    assertEqual(branchItems.map((item) => item.line), [0], 'closed on one branch only');

    const breakItems = summary(await pullUnclosed(session, await openFixture(session, 'break_open.prg')));
    assertEqual(breakItems.length, 1, 'open at break count');
    assert(breakItems[0].message.includes('Database 1 is opened here and still open at break (line 3)'),
        `open at break message: ${breakItems[0].message}`);
    assertEqual(breakItems[0].related, ['break_open.prg:2'], 'open at break links the break');

    assertEqual(await pullUnclosed(session, await openFixture(session, 'break_ignore.prg')), [],
        'IGNORE CLOSE on the break line');
    assertEqual(await pullUnclosed(session, await openFixture(session, 'dbvalid_guard.prg')), [],
        'failed dbvalid branch counts as not open');

    const scopeItems = summary(await pullUnclosed(session, await openFixture(session, 'section_scope.prg')));
    assertEqual(scopeItems.map((item) => item.line), [1], 'close in another section does not count');

    const noCallerItems = summary(await pullUnclosed(session, await openFixture(session, 'no_caller.prg')));
    assertEqual(noCallerItems.length, 1, 'no caller count');
    assert(noCallerItems[0].message.includes('is opened here for the caller, but no caller closes it'),
        `no caller message: ${noCallerItems[0].message}`);

    const noCommons = summary(await pullUnclosed(session, await openFixture(session, 'chain_nocommons.prg')));
    assertEqual(noCommons.map((item) => item.line), [2, 6], 'caller without common_dbases cannot take over');
    assertEqual(noCommons[0].related, ['chain_nocommons.prg:6'], 'no-commons open links the call');

    const loopItems = summary(await pullUnclosed(session, await openFixture(session, 'loops.prg')));
    assertEqual(loopItems.map((item) => item.line), [1], 'loop body may be skipped; balanced loop is clean');

    // A called section's warning follows its caller's edits.
    const openerUri = await openFixture(session, 'cross_opener.prg');
    const callerUri = await openFixture(session, 'cross_caller.prg');
    await session.waitQuiet(400);
    const crossBefore = summary(await pullUnclosed(session, openerUri));
    assertEqual(crossBefore.map((item) => item.line), [2], 'cross-file callee warns before the fix');
    assertEqual(crossBefore[0].related, ['cross_caller.prg:2'], 'cross-file callee links the caller');
    assertEqual(summary(await pullUnclosed(session, callerUri)).map((item) => item.line), [2],
        'cross-file caller warns on its call line');
    session.didChange({
        uri: callerUri,
        version: 2,
        contentChanges: [{ text: `${fixtureText('cross_caller.prg')}dbclose(d)\n` }],
    });
    await session.waitQuiet(400);
    assertEqual(await pullUnclosed(session, callerUri), [], 'cross-file caller clean after the fix');
    assertEqual(await pullUnclosed(session, openerUri), [], 'cross-file callee clean after the caller fix');
}

async function run() {
    await withSession(runGoldens);
    console.log('lsp golden: 20-unclosed-resources passed');
}

if (require.main === module) {
    run().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run, runGoldens };
