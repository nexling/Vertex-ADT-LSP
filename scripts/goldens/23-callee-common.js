'use strict';

/**
 * Family 13 callee common-requirements goldens.
 * Assertions from test-fixtures/workspace-declaration-index/TESTING.md scenario 14
 * plus the section-scoped / skip rules that ticket cites. Do not rewrite TESTING.md.
 *
 *   node scripts/goldens/23-callee-common.js
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'workspace-declaration-index');
const CUSTOM = path.join(FIXTURE, 'custom');

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

function readCustom(name) {
    return fs.readFileSync(path.join(CUSTOM, name), 'utf8');
}

function uriFor(name) {
    return pathToFileURL(path.join(CUSTOM, name)).href;
}

function firstLine(text) {
    return text.split('\n')[0];
}

function wrappedMessage(lineText, inner) {
    return `${lineText} has an error: ${inner}`;
}

function innerFor(basename, directive, marker, locationHint) {
    return `call() to ${basename} requires ${directive} 1 ${locationHint} (declared via ${marker}).`;
}

function findCalleeCommon(items) {
    return (items || []).filter((item) =>
        typeof item.message === 'string' && item.message.includes('requires common_')
    );
}

function assertWarning(diag, message, line, endCharacter, relatedInner) {
    assertEqual(diag.severity, 2, `${message} severity`);
    assertEqual(diag.source, 'ADT-Syntax', `${message} source`);
    assert(diag.code === undefined || diag.code === null, `${message} has no code`);
    assertEqual(diag.message, message, 'diagnostic message');
    assertEqual(diag.range && diag.range.start, { line, character: 1 }, `${message} range start`);
    assertEqual(diag.range && diag.range.end, { line, character: endCharacter }, `${message} range end`);
    const related = diag.relatedInformation || [];
    assert(related.length >= 1, `${message} relatedInformation`);
    assertEqual(related[0].message, relatedInner, `${message} related inner`);
}

function launchSession() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    return LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'workspace-declaration-index' }],
    });
}

async function withSession(fn) {
    const session = launchSession();
    try {
        await session.initialize();
        await fn(session);
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

async function openFile(session, name, text) {
    session.didOpen({
        uri: uriFor(name),
        languageId: 'vertexadt',
        version: 1,
        text: text === undefined ? readCustom(name) : text,
    });
}

async function indexCallees(session) {
    await openFile(session, 'callee_with_common.prg');
    await openFile(session, 'callee_no_common.prg');
    await session.waitQuiet(400);
    await session.hover(uriFor('callee_with_common.prg'), 0, 0);
    await session.hover(uriFor('callee_no_common.prg'), 0, 0);
}

async function run() {
    const missingText = readCustom('caller_missing_common.prg');
    const missingLine = firstLine(missingText);
    const missingInner = innerFor(
        'callee_with_common.prg',
        'common_indexes',
        '@require_common_indexes',
        'in this file'
    );
    const missingMessage = wrappedMessage(missingLine, missingInner);
    const jointCallLine = 'ok = call("callee_with_common.prg")';
    const jointInner = innerFor(
        'callee_with_common.prg',
        'common_indexes',
        '@require_common_indexes',
        'in #JOINT_TEST#'
    );

    await withSession(async (session) => {
        await openFile(session, 'caller_missing_common.prg', missingText);
        const beforeIndex = await session.diagnostic(uriFor('caller_missing_common.prg'));
        assertEqual(beforeIndex && beforeIndex.kind, 'full', 'no-disk-fallback pull kind');
        assertEqual(
            findCalleeCommon(beforeIndex.items),
            [],
            'scenario 14: missing index entry is not a disk fallback'
        );
    });

    await withSession(async (session) => {
        await indexCallees(session);

        await openFile(session, 'caller_missing_common.prg', missingText);
        await openFile(session, 'caller_with_common.prg');
        await openFile(session, 'caller_no_common.prg');
        await session.waitQuiet(400);
        await session.hover(uriFor('caller_missing_common.prg'), 0, 0);

        const missingPull = await session.diagnostic(uriFor('caller_missing_common.prg'));
        assertEqual(missingPull && missingPull.kind, 'full', 'caller_missing_common pull kind');
        const missingItems = findCalleeCommon(missingPull.items);
        assertEqual(missingItems.length, 1, 'scenario 14 steps 1–3: one warning on call()');
        assertWarning(missingItems[0], missingMessage, 0, missingLine.length, missingInner);

        const withPull = await session.diagnostic(uriFor('caller_with_common.prg'));
        assertEqual(
            findCalleeCommon(withPull.items),
            [],
            'scenario 14 steps 4–5: common_indexes 1 satisfies the callee marker'
        );

        const nonePull = await session.diagnostic(uriFor('caller_no_common.prg'));
        assertEqual(
            findCalleeCommon(nonePull.items),
            [],
            'scenario 14 steps 6–7: callee without @require_common_* imposes nothing'
        );

        const jointMissingText = [
            'common_indexes 1',
            '#JOINT_TEST#',
            'ok = call("callee_with_common.prg")',
            '#JOINT_TEST#',
            '',
        ].join('\n');
        await openFile(session, '_golden_joint_missing.prg', jointMissingText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_joint_missing.prg'), 0, 0);
        const jointMissingPull = await session.diagnostic(uriFor('_golden_joint_missing.prg'));
        const jointMissingItems = findCalleeCommon(jointMissingPull.items);
        assertEqual(jointMissingItems.length, 1, 'scenario 14.8: file-level common_* does not satisfy a JOINT_TEST call');
        assertWarning(
            jointMissingItems[0],
            wrappedMessage(jointCallLine, jointInner),
            2,
            jointCallLine.length,
            jointInner
        );

        const jointOkText = [
            '#JOINT_TEST#',
            'common_indexes 1',
            'ok = call("callee_with_common.prg")',
            '#JOINT_TEST#',
            '',
        ].join('\n');
        await openFile(session, '_golden_joint_ok.prg', jointOkText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_joint_ok.prg'), 0, 0);
        const jointOkPull = await session.diagnostic(uriFor('_golden_joint_ok.prg'));
        assertEqual(
            findCalleeCommon(jointOkPull.items),
            [],
            'scenario 14.8: common_indexes 1 in JOINT_TEST satisfies a JOINT_TEST call'
        );

        const fileLevelFromSectionText = [
            '#JOINT_TEST#',
            'common_indexes 1',
            '#JOINT_TEST#',
            'ok = call("callee_with_common.prg")',
            '',
        ].join('\n');
        await openFile(session, '_golden_file_from_section.prg', fileLevelFromSectionText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_file_from_section.prg'), 0, 0);
        const fileFromSectionPull = await session.diagnostic(uriFor('_golden_file_from_section.prg'));
        const fileFromSectionItems = findCalleeCommon(fileFromSectionPull.items);
        assertEqual(fileFromSectionItems.length, 1, 'scenario 14.8: section common_* does not satisfy a file-level call');
        assertWarning(
            fileFromSectionItems[0],
            wrappedMessage(jointCallLine, missingInner),
            3,
            jointCallLine.length,
            missingInner
        );

        const namedTargetText = [
            '#JOINT_TEST#',
            'ok = call("callee_with_common.prg", JOINT_TEST)',
            '#JOINT_TEST#',
            '',
        ].join('\n');
        await openFile(session, '_golden_named_target.prg', namedTargetText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_named_target.prg'), 0, 0);
        const namedTargetPull = await session.diagnostic(uriFor('_golden_named_target.prg'));
        const namedTargetItems = findCalleeCommon(namedTargetPull.items);
        const namedLine = 'ok = call("callee_with_common.prg", JOINT_TEST)';
        assertEqual(namedTargetItems.length, 1, 'scenario 14.8: named target still applies file-level callee markers');
        assertWarning(
            namedTargetItems[0],
            wrappedMessage(namedLine, jointInner),
            1,
            namedLine.length,
            jointInner
        );

        const zeroText = 'common_indexes 0\nok = call("callee_with_common.prg")\n';
        await openFile(session, '_golden_common_zero.prg', zeroText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_common_zero.prg'), 0, 0);
        const zeroPull = await session.diagnostic(uriFor('_golden_common_zero.prg'));
        const zeroItems = findCalleeCommon(zeroPull.items);
        assertEqual(zeroItems.length, 1, 'common_indexes 0 does not satisfy');
        assertWarning(
            zeroItems[0],
            wrappedMessage(jointCallLine, missingInner),
            1,
            jointCallLine.length,
            missingInner
        );

        const tenText = 'common_indexes 10\nok = call("callee_with_common.prg")\n';
        await openFile(session, '_golden_common_ten.prg', tenText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_common_ten.prg'), 0, 0);
        const tenPull = await session.diagnostic(uriFor('_golden_common_ten.prg'));
        assertEqual(findCalleeCommon(tenPull.items).length, 1, 'common_indexes 10 does not satisfy');

        const includeText = '# include "callee_with_common.prg"\n';
        await openFile(session, '_golden_include_not_call.prg', includeText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_include_not_call.prg'), 0, 0);
        const includePull = await session.diagnostic(uriFor('_golden_include_not_call.prg'));
        assertEqual(findCalleeCommon(includePull.items), [], '# include is not a call site');

        const bangText = '! ok = call("callee_with_common.prg")\n';
        await openFile(session, '_golden_bang_call.prg', bangText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_bang_call.prg'), 0, 0);
        const bangPull = await session.diagnostic(uriFor('_golden_bang_call.prg'));
        assertEqual(findCalleeCommon(bangPull.items), [], 'bang whole-line is not a call site');

        const editFormText = [
            '#EDIT_FORM#',
            'ok = call("callee_with_common.prg")',
            '#EDIT_FORM#',
            '',
        ].join('\n');
        await openFile(session, '_golden_edit_form.prg', editFormText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_edit_form.prg'), 0, 0);
        const editFormPull = await session.diagnostic(uriFor('_golden_edit_form.prg'));
        assertEqual(findCalleeCommon(editFormPull.items), [], '#EDIT_FORM# call site is skipped');

        const spaceText = 'call callee_with_common.prg\n';
        await openFile(session, '_golden_space_form.prg', spaceText);
        await session.waitQuiet(200);
        await session.hover(uriFor('_golden_space_form.prg'), 0, 0);
        const spacePull = await session.diagnostic(uriFor('_golden_space_form.prg'));
        const spaceItems = findCalleeCommon(spacePull.items);
        const spaceLine = 'call callee_with_common.prg';
        assertEqual(spaceItems.length, 1, 'space-form call is a site');
        assertWarning(
            spaceItems[0],
            wrappedMessage(spaceLine, missingInner),
            0,
            spaceLine.length,
            missingInner
        );
    });
}

if (require.main === module) {
    run().then(() => {
        console.log('lsp golden: 23-callee-common passed');
    }).catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run };
