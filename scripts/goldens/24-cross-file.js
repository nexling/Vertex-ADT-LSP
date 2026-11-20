'use strict';

/**
 * Parity goldens for family 14 cross-file declaration conflicts.
 * Assertions follow test-fixtures/workspace-declaration-index/TESTING.md
 * scenarios 1–9, 12, 15 (scenario 5 pins strict off).
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const REPO_ROOT = path.resolve(__dirname, '..', '..');
const FIXTURE = path.join(REPO_ROOT, 'test-fixtures', 'workspace-declaration-index');
const CUSTOM = path.join(FIXTURE, 'custom');
const SYSTEM = path.join(FIXTURE, 'system');

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

function customUri(name) {
    return pathToFileURL(path.join(CUSTOM, name)).href;
}

function systemUri(name) {
    return pathToFileURL(path.join(SYSTEM, name)).href;
}

function readCustom(name) {
    return fs.readFileSync(path.join(CUSTOM, name), 'utf8');
}

function listCustomPrg() {
    return fs.readdirSync(CUSTOM).filter((name) => name.toLowerCase().endsWith('.prg'));
}

function pullItems(report) {
    return (report && report.items) || [];
}

function crossFileItems(items) {
    return (items || []).filter((diagnostic) => {
        const message = diagnostic && diagnostic.message;
        return diagnostic
            && diagnostic.source === 'ADT-Syntax'
            && diagnostic.severity === 3
            && typeof message === 'string'
            && message.includes('shares declaration number');
    });
}

function messages(items) {
    return crossFileItems(items).map((diagnostic) => diagnostic.message);
}

function findMessage(items, snippet) {
    return messages(items).find((message) => message.includes(snippet));
}

function relatedMessages(diagnostic) {
    return ((diagnostic && diagnostic.relatedInformation) || []).map((item) => item.message);
}

async function openAllCustom(session) {
    for (const name of listCustomPrg()) {
        session.didOpen({
            uri: customUri(name),
            languageId: 'vertexadt',
            version: 1,
            text: readCustom(name),
        });
    }
    session.didOpen({
        uri: systemUri('custom_override.prg'),
        languageId: 'vertexadt',
        version: 1,
        text: fs.readFileSync(path.join(SYSTEM, 'custom_override.prg'), 'utf8'),
    });
    await session.waitQuiet(400);
}

async function pull(session, name) {
    return pullItems(await session.diagnostic(customUri(name), 15000));
}

async function run() {
    const rootUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(REPO_ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'workspace-declaration-index' }],
    });
    try {
        await session.initialize();
        await openAllCustom(session);

        const offMain = await pull(session, 'main_include.prg');
        const offSub = await pull(session, 'sub_included.prg');
        assertEqual(crossFileItems(offMain).length, 0, 'scenario 5 main_include strict off');
        assertEqual(crossFileItems(offSub).length, 0, 'scenario 5 sub_included strict off');

        session.didChangeConfiguration({
            vertexadt: { strictMode: { enabled: true } },
        });
        await session.waitQuiet(300);

        const main = await pull(session, 'main_include.prg');
        const sub = await pull(session, 'sub_included.prg');
        const mainMsg = findMessage(main, 'Variable VarA shares declaration number 10 with VarB in sub_included.prg');
        const subMsg = findMessage(sub, 'Variable VarB shares declaration number 10 with VarA in main_include.prg');
        assert(mainMsg, `scenario 1 main_include: ${JSON.stringify(messages(main))}`);
        assert(subMsg, `scenario 1 sub_included: ${JSON.stringify(messages(sub))}`);
        assert(mainMsg.includes('sub_included.prg is linked through main_include.prg on line 1 (# include).'), 'scenario 1 include chain');
        assert(mainMsg.includes('Next available: 11'), `scenario 1 next available: ${mainMsg}`);
        assert(!mainMsg.includes('[scope:'), 'scenario 1 has one scope');
        assertEqual(crossFileItems(main).length, 1, 'scenario 1 one diagnostic on main_include');

        const mainDiag = crossFileItems(main)[0];
        assertEqual(mainDiag.code, undefined, 'scenario 1 no code');
        assertEqual(mainDiag.range.start.line, 3, 'scenario 1 range line');
        assertEqual(mainDiag.range.start.character, 1, 'scenario 1 wrapped offset+1');
        const related = relatedMessages(mainDiag);
        assert(related.includes('VarB = 10 in sub_included.prg'), `scenario 6 peer related: ${JSON.stringify(related)}`);
        assert(
            related.some((item) => item.includes('# include in main_include.prg line 1 → sub_included.prg')),
            `scenario 6 chain related: ${JSON.stringify(related)}`
        );
        const peerRelated = (mainDiag.relatedInformation || []).find((item) => item.message === 'VarB = 10 in sub_included.prg');
        assert(peerRelated && peerRelated.location && typeof peerRelated.location.uri === 'string', 'scenario 6 related uri');
        assert(
            peerRelated.location.uri.toLowerCase().includes('sub_included.prg'),
            `scenario 6 related uri is sub_included: ${peerRelated.location.uri}`
        );

        const noCommonCaller = await pull(session, 'caller_no_common.prg');
        const noCommonCallee = await pull(session, 'callee_no_common.prg');
        assertEqual(crossFileItems(noCommonCaller).length, 0, 'scenario 2 caller_no_common');
        assertEqual(crossFileItems(noCommonCallee).length, 0, 'scenario 2 callee_no_common');

        const withCommon = await pull(session, 'caller_with_common.prg');
        const calleeCommon = await pull(session, 'callee_with_common.prg');
        const sharedA = findMessage(withCommon, 'Variable SharedA shares declaration number 20 with SharedB in callee_with_common.prg');
        const sharedB = findMessage(calleeCommon, 'Variable SharedB shares declaration number 20 with SharedA in caller_with_common.prg');
        assert(sharedA, `scenario 3 caller: ${JSON.stringify(messages(withCommon))}`);
        assert(sharedB, `scenario 3 callee: ${JSON.stringify(messages(calleeCommon))}`);
        assert(sharedA.includes('(call())'), 'scenario 3 call() chain label');
        assert(sharedA.includes('Next available: 21'), `scenario 3 next available: ${sharedA}`);

        const override = await pull(session, 'custom_override.prg');
        assertEqual(crossFileItems(override).length, 0, 'scenario 4 custom-only no conflict');
        assert(
            messages(override).every((message) => !/systemonly|system\\\\custom_override|system\/custom_override/i.test(message)),
            'scenario 4 does not mention system twin'
        );
        const systemPull = pullItems(await session.diagnostic(systemUri('custom_override.prg'), 15000));
        assertEqual(
            crossFileItems(systemPull).length,
            0,
            'scenario 4 system twin is not a conflict slot'
        );

        const callerA = await pull(session, 'caller_a.prg');
        const callerB = await pull(session, 'caller_b.prg');
        const hub = await pull(session, 'shared_callee.prg');
        assert(findMessage(callerA, 'Variable CallerAVar shares declaration number 10 with SharedCalleeVar in shared_callee.prg'), `scenario 7 caller_a: ${JSON.stringify(messages(callerA))}`);
        assert(findMessage(callerB, 'Variable CallerBVar shares declaration number 10 with SharedCalleeVar in shared_callee.prg'), `scenario 7 caller_b: ${JSON.stringify(messages(callerB))}`);
        assert(!findMessage(callerA, 'caller_b.prg'), 'scenario 7 caller_a must not conflict with caller_b');
        assert(!findMessage(callerB, 'caller_a.prg'), 'scenario 7 caller_b must not conflict with caller_a');
        const hubA = findMessage(hub, '[scope: caller_a.prg]');
        const hubB = findMessage(hub, '[scope: caller_b.prg]');
        assert(hubA, `scenario 7 hub scope caller_a: ${JSON.stringify(messages(hub))}`);
        assert(hubB, `scenario 7 hub scope caller_b: ${JSON.stringify(messages(hub))}`);
        assert(hubA.includes('CallerAVar'), 'scenario 7 hub vs caller_a name');
        assert(hubB.includes('CallerBVar'), 'scenario 7 hub vs caller_b name');

        const includeA = await pull(session, 'include_caller_a.prg');
        const includeB = await pull(session, 'include_caller_b.prg');
        const includeHub = await pull(session, 'shared_include.prg');
        assert(findMessage(includeA, 'Variable IncludeAVar shares declaration number 20 with SharedIncludeVar in shared_include.prg'), `scenario 8 include_caller_a: ${JSON.stringify(messages(includeA))}`);
        assert(findMessage(includeB, 'Variable IncludeBVar shares declaration number 20 with SharedIncludeVar in shared_include.prg'), `scenario 8 include_caller_b: ${JSON.stringify(messages(includeB))}`);
        assert(!findMessage(includeA, 'include_caller_b.prg'), 'scenario 8 include_caller_a must not conflict with include_caller_b');
        assert(findMessage(includeHub, '[scope: include_caller_a.prg]'), `scenario 8 hub scope a: ${JSON.stringify(messages(includeHub))}`);
        assert(findMessage(includeHub, '[scope: include_caller_b.prg]'), `scenario 8 hub scope b: ${JSON.stringify(messages(includeHub))}`);

        const valueCaller = await pull(session, 'caller_value_skip.prg');
        const valueCallee = await pull(session, 'shared_callee_value.prg');
        const objectMsg = findMessage(valueCaller, 'Variable ObjectSlot shares declaration number 30 with CalleeObject in shared_callee_value.prg');
        assert(objectMsg, `scenario 9 object conflict: ${JSON.stringify(messages(valueCaller))}`);
        assert(!findMessage(valueCaller, 'ValueSlot'), 'scenario 9 ValueSlot skipped on caller');
        assert(!findMessage(valueCallee, 'ValueSlot'), 'scenario 9 ValueSlot skipped on callee');
        assert(objectMsg.includes('Next available: 31'), `scenario 9 next available: ${objectMsg}`);

        const sectionCaller = await pull(session, 'section_include_caller.prg');
        const sectionCallee = await pull(session, 'section_include_callee.prg');
        const joint = findMessage(sectionCaller, 'Variable CallerVar shares declaration number 10 with CalleeVar in section_include_callee.prg');
        assert(joint, `scenario 12 caller: ${JSON.stringify(messages(sectionCaller))}`);
        assert(joint.includes('[section: JOINT]'), `scenario 12 JOINT section: ${joint}`);
        assert(!findMessage(sectionCaller, 'CallerOther'), 'scenario 12 CallerOther in OTHER must not conflict');
        assert(!findMessage(sectionCaller, 'OtherVar'), 'scenario 12 OtherVar must not meet caller OTHER');
        assert(findMessage(sectionCallee, 'Variable CalleeVar shares declaration number 10 with CallerVar in section_include_caller.prg'), `scenario 12 callee: ${JSON.stringify(messages(sectionCallee))}`);

        const sameCaller = await pull(session, 'caller_same_slot.prg');
        const sameCallee = await pull(session, 'callee_same_slot.prg');
        assertEqual(crossFileItems(sameCaller).length, 0, 'scenario 15 aligned extravar=105 on caller');
        assertEqual(crossFileItems(sameCallee).length, 0, 'scenario 15 aligned extravar=105 on callee');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    run().then(() => {
        console.log('24-cross-file goldens passed');
    }).catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run };
