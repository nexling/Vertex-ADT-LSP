'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'joint-section-rules');

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

function wrap(lineText, inner) {
    return `${lineText} has an error: ${inner}`;
}

function lineIndex(lines, text) {
    const i = lines.indexOf(text);
    if (i < 0) {
        throw new Error(`missing fixture line ${JSON.stringify(text)}`);
    }
    return i;
}

function openPull(session, fileName) {
    const filePath = path.join(FIXTURE, fileName);
    const text = fs.readFileSync(filePath, 'utf8');
    const uri = pathToFileURL(filePath).href;
    const lines = text.split('\n').map((line) => line.replace(/\r$/, ''));
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    return session.diagnostic(uri).then((pull) => {
        assert(pull && pull.kind === 'full', `${fileName} pull kind`);
        assert(pull.relatedDocuments === undefined, `${fileName} has no relatedDocuments`);
        return { uri, items: pull.items || [], lines };
    });
}

function keyOf(line, message) {
    return `${line}|${message}`;
}

function isJointSectionDiagnostic(item) {
    const message = String(item && item.message);
    if (item.source !== 'ADT-Syntax' || !message.includes(' has an error: ')) {
        return false;
    }
    return message.includes('not allowed in') || message.includes('if-then-exit failsafe');
}

function assertJointSet(items, expected, label) {
    const jointItems = items.filter(isJointSectionDiagnostic);
    const actualKeys = jointItems.map((d) => keyOf(d.range.start.line, d.message)).sort();
    const expectedKeys = expected.map((e) => keyOf(e.line, wrap(e.lineText, e.inner))).sort();
    assertEqual(actualKeys, expectedKeys, `${label} messages`);

    for (const item of jointItems) {
        assertEqual(item.severity, 2, `${label} line ${item.range.start.line} severity`);
        assertEqual(item.source, 'ADT-Syntax', `${label} line ${item.range.start.line} source`);
        assert(
            item.code === undefined || item.code === null,
            `${label} line ${item.range.start.line} has no code`
        );
        assertEqual(
            item.range.start.line,
            item.range.end.line,
            `${label} line ${item.range.start.line} same-line range`
        );
        const inner = String(item.message).replace(/^.* has an error: /, '');
        if (item.relatedInformation && item.relatedInformation.length > 0) {
            assertEqual(
                item.relatedInformation[0].message,
                inner,
                `${label} line ${item.range.start.line} related inner`
            );
        }
    }
}

function expect(lines, text, inner) {
    return { line: lineIndex(lines, text), lineText: text, inner };
}

async function runTableRows(session) {
    const { items, lines } = await openPull(session, 'table-rows.prg');
    assertJointSet(items, [
        expect(lines, 'JT_MACRO=new_macro()', 'new_macro is not allowed in JOINT_TEST.'),
        expect(lines, 'JT_JOINT=new_djoint()', 'new_djoint is not allowed in JOINT_TEST.'),
        expect(lines, 'JT_BOTH=new_macro() new_djoint()', 'new_macro is not allowed in JOINT_TEST.'),
        expect(
            lines,
            'UJ_MACRO=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(lines, 'UJ_JOINT=new_djoint()', 'new_djoint is not allowed in UPDATE_JOINT.'),
        expect(lines, 'UJ_BOTH=new_djoint() new_macro()', 'new_djoint is not allowed in UPDATE_JOINT.'),
        expect(
            lines,
            'JD_MACRO=new_macro()',
            'new_macro in JOINT_DETAILING requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'JA_MACRO=new_macro()',
            'new_macro in JOINT_ADAPT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(lines, 'EP_JOINT=new_djoint()', 'new_djoint is not allowed in EDIT_PROPERTIES.'),
    ], 'table-rows');
}

async function runFailsafe(session) {
    const { items, lines } = await openPull(session, 'failsafe.prg');
    assertJointSet(items, [
        expect(
            lines,
            'FS_COMPACT=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_TAB=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_SETCASE=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_RETCASE=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            "FS_SINGLEQ=new_macro()",
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_EXITFIRST=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_ENDIFFIRST=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_SPLITIF=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_AFTER=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_SAMELINE=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'FS_ONHEADER=new_macro()',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
    ], 'failsafe');
}

async function runSkipParse(session) {
    const { items, lines } = await openPull(session, 'skip-parse.prg');
    const nestLine = lineIndex(lines, 'NEST_MACRO=new_macro()');
    const nestText = 'NEST_MACRO=new_macro()';
    assertJointSet(items, [
        expect(
            lines,
            'BANG_SEMI=new_macro();!failsafe?',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'MSG_MACRO=messagebox new_macro',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        expect(
            lines,
            'QUOTED=x = "new_macro"',
            'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.'
        ),
        { line: nestLine, lineText: nestText, inner: 'new_macro is not allowed in JOINT_TEST.' },
        {
            line: nestLine,
            lineText: nestText,
            inner: 'new_macro in UPDATE_JOINT requires an if-then-exit failsafe earlier in this section.',
        },
    ], 'skip-parse');
}

async function run(session) {
    await session.initialize();
    await runTableRows(session);
    await runFailsafe(session);
    await runSkipParse(session);
    await session.shutdownAndExit();
}

async function main() {
    const session = LspSession.launch();
    try {
        await run(session);
    } catch (err) {
        session.kill();
        throw err;
    }
    console.log('lsp golden 25-joint-section: passed');
}

if (require.main === module) {
    main().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run: main };
