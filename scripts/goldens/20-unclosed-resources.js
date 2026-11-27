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
        || item.code === 'vertexadt/close-already-open'
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

    // The source program owns an ALREADY_OPEN alias and closes it.
    const alreadyUri = await openFixture(session, 'already_open.prg');
    assertEqual(await pullUnclosed(session, alreadyUri), [], 'ALREADY_OPEN needs no close');

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
    await runDrawingGoldens(session);
    const aliasCloseUri = await runAliasCloseGoldens(session);

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
        uri: aliasCloseUri,
        version: 2,
        contentChanges: [{ text: ALIAS_CLOSE }],
    });
    assertEqual(await pullUnclosed(session, aliasCloseUri), [], 'setting off skips the walk');

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

// Native-only (docs/adr/0029-native-background-drawing-unclosed-resources.md):
// dwgopen_bg opens a drawing that dwgclose must close. The handle is the port
// expression, matched by text without spaces outside quotes, ignoring case.
async function runDrawingGoldens(session) {
    const lines = async (name, text) => summary(await pullUnclosed(session, await openText(session, name, text)));
    const OPEN = (port) => `Ok=dwgopen_bg(${port},File$,1,1,20,108)\n`;

    assertEqual(await lines('dwg_closed.prg', `${OPEN('"planlay"')}dwgclose "planlay"\n`), [],
        'dwgopen_bg closed by dwgclose');

    const unclosed = await lines('dwg_unclosed.prg', OPEN('"planlay"'));
    assertEqual(unclosed.map((item) => item.line), [0], 'unclosed dwgopen_bg');
    assertEqual(unclosed[0].message, 'Drawing "planlay" is opened here and not closed in this execution tree',
        'unclosed drawing message');

    assertEqual(await lines('dwg_case.prg', `${OPEN(' "PlanLay" ')}dwgclose  "planlay"\n`), [],
        'literal port matches ignoring case and spaces');
    assertEqual(await lines('dwg_var.prg', `${OPEN('Win$')}dwgclose win$\n`), [],
        'variable port matches ignoring case');
    assertEqual(await lines('dwg_expr.prg', `${OPEN('"tmp"&Nr$')}dwgclose "tmp" & nr$\n`), [],
        'composite port matches as normalized text');
    assertEqual(await lines('dwg_parens.prg', `${OPEN('"p"')}dwgclose("p")\n`), [],
        'dwgclose with parentheses');
    assertEqual(await lines('dwg_semicolon.prg', `${OPEN('"p"')}dwgclose "p"; x = 1\n`), [],
        'dwgclose ends at ;');
    assertEqual((await lines('dwg_literal_var.prg', `${OPEN('"planlay"')}dwgclose Win$\n`)).map((item) => item.line),
        [0], 'a literal never matches a variable');

    assertEqual(await lines('dwg_untracked.prg',
        'Ok = dwgopen("PICTURE",F$,10,10,35,100)\nmodelopen "M",model=F$\nOk=dwgopen_old("D")\n'),
    [], 'dwgopen, modelopen and dwgopen_old are not tracked');
    assertEqual(await lines('dwg_close_only.prg', 'dwgclose "PICTURE"\n'), [], 'close without open is silent');

    assertEqual(await lines('dwg_guard.prg', `${OPEN('"p"')}if !Ok then\n    exit\nendif\ndwgclose "p"\n`), [],
        'failed open branch counts as not open');
    assertEqual(await lines('dwg_guard_eq.prg', `${OPEN('"p"')}if Ok = 0 then exit\ndwgclose "p"\n`), [],
        'Ok = 0 inline guard');

    const atExit = await lines('dwg_exit.prg', `${OPEN('"p"')}if x then\n    exit\nendif\ndwgclose "p"\n`);
    assertEqual(atExit.map((item) => item.line), [0], 'open at exit');
    assert(atExit[0].message.includes('Drawing "p" is opened here and still open at exit (line 3)'),
        `open at exit message: ${atExit[0].message}`);

    assertEqual((await lines('dwg_branch.prg', `${OPEN('"p"')}if x then\n    dwgclose "p"\nendif\n`))
        .map((item) => item.line), [0], 'closed on one branch only');
    assertEqual(await lines('dwg_reopen.prg', `${OPEN('"p"')}${OPEN('"p"')}dwgclose "p"\n`), [],
        'reopening an open port is silent');
    assertEqual(await lines('dwg_ignore.prg', 'Ok=dwgopen_bg("p",F$,1,1,20,108) ;!IGNORE CLOSE\n'), [],
        'IGNORE CLOSE on the open');

    // A literal port is the same drawing in every frame: the duty goes up the call chain.
    const chain = summary(await pullUnclosed(session, await openFixture(session, 'drawing_chain.prg')));
    assertEqual(chain.map((item) => item.line), [1, 10], 'drawing chain: open and failing call line');
    assert(chain[0].message.includes('Drawing "planlay" is opened here and not closed on every caller path'),
        `drawing chain open message: ${chain[0].message}`);
    assertEqual(chain[0].related, ['drawing_chain.prg:10'], 'drawing chain open links only the failing caller');
    assert(chain[1].message.includes('Drawing "planlay" opened by the called code is not closed on every path after this call'),
        `drawing chain call-line message: ${chain[1].message}`);
}

const ALIAS_CLOSE = 'dbopen(1) "ALREADY_OPEN",X\ndbclose(1)\n';
const ALIAS_MESSAGE = "dbclose closes the source program's database";

// Native-only: a dbclose of a handle whose latest dbopen on some path was
// "ALREADY_OPEN" closes the source program's database.
async function runAliasCloseGoldens(session) {
    const aliasCloseUri = await openText(session, 'alias_close.prg', ALIAS_CLOSE);
    const report = await session.diagnostic(aliasCloseUri);
    const items = unclosed(report.items);
    assertEqual(items.length, 1, 'alias close count');
    assertEqual(items[0].code, 'vertexadt/close-already-open', 'alias close code');
    assertEqual(items[0].severity, 2, 'alias close warning');
    assertEqual(items[0].range.start.line, 1, 'alias close on the dbclose line');
    assertEqual(messageOf(items[0]),
        `Database 1 was aliased with ALREADY_OPEN (line 1); ${ALIAS_MESSAGE}`, 'alias close message');
    assertEqual(summary(items)[0].related, ['alias_close.prg:0'], 'alias close links the alias');

    const lines = async (name, text) => summary(await pullUnclosed(session, await openText(session, name, text)));

    assertEqual(await lines('alias_function_form.prg',
        'Cid = dbopen(Id,"ALREADY_OPEN",Cid)\n'),
    [], 'function-form ALREADY_OPEN needs no close');
    assertEqual((await lines('alias_function_close.prg',
        'Cid = dbopen(Id,"ALREADY_OPEN",Cid)\ndbclose(Id)\n')).map((item) => item.line),
    [1], 'function-form alias close warns');

    assertEqual(await lines('alias_then_real.prg',
        'dbopen(1) "ALREADY_OPEN",X\ndbopen(1) "A",1\ndbclose(1)\n'),
    [], 'latest open is real: no alias warning');

    const realThenAlias = await lines('real_then_alias.prg',
        'dbopen(1) "A",1\ndbopen(1) "ALREADY_OPEN",X\ndbclose(1)\n');
    assertEqual(realThenAlias.map((item) => item.line), [2],
        'latest open is the alias: warn on dbclose, real open counts as closed');
    assert(realThenAlias[0].message.includes(ALIAS_MESSAGE), `real then alias: ${realThenAlias[0].message}`);

    const mixed = await lines('alias_mixed.prg',
        'if x then\n    dbopen(1) "ALREADY_OPEN",X\nelse\n    dbopen(1) "A",1\nendif\ndbclose(1)\n');
    assertEqual(mixed.map((item) => item.line), [5], 'alias on one branch: warn on dbclose only');

    assertEqual(await lines('alias_ignore.prg',
        'dbopen(1) "ALREADY_OPEN",X\ndbclose(1) ;!IGNORE CLOSE\n'),
    [], 'IGNORE CLOSE on the dbclose line');

    assertEqual(await lines('alias_by_name.prg',
        'dbopen(1) "ALREADY_OPEN",X\nok = dbclose_by_name(\'X\')\n'),
    [], 'dbclose_by_name is not tied to an alias');

    // Alias in the caller, dbclose in the callee.
    await openFixture(session, 'alias_caller.prg');
    const closerUri = await openFixture(session, 'alias_closer.prg');
    await session.waitQuiet(400);
    const closer = summary(await pullUnclosed(session, closerUri));
    assertEqual(closer.map((item) => item.line), [0], 'callee dbclose of caller alias');
    assert(closer[0].message.includes('(alias_caller.prg line 2)'), `callee close message: ${closer[0].message}`);
    assertEqual(closer[0].related, ['alias_caller.prg:1'], 'callee close links the caller alias');
    assertEqual(await pullUnclosed(session, fixtureUri('alias_caller.prg')), [], 'alias caller is clean');

    // Alias in the callee, dbclose in the caller.
    const reverse = summary(await pullUnclosed(session, await openFixture(session, 'alias_reverse_caller.prg')));
    assertEqual(reverse.map((item) => item.line), [2], 'caller dbclose of callee alias');
    assert(reverse[0].message.includes('(alias_opener_callee.prg line 1)'),
        `caller close message: ${reverse[0].message}`);

    return aliasCloseUri;
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
