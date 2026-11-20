'use strict';

/**
 * Parity goldens for family 8 unknown-section.
 * Assertions from include-call-sections suffix/unknown pins plus uniquely
 * named fixtures. Do not rewrite TESTING.md.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'include-call-sections');
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

function linesOf(text) {
    return text.split('\n').map((line) => line.replace(/\r$/, ''));
}

function lineIndex(lines, text) {
    const i = lines.indexOf(text);
    if (i < 0) {
        throw new Error(`missing fixture line ${JSON.stringify(text)}`);
    }
    return i;
}

function unknownItems(items) {
    return (items || []).filter((item) => item && item.code === 'vertexadt/unknown-section');
}

function underline(lineText, range) {
    return lineText.slice(range.start.character, range.end.character);
}

function msg(section, basename) {
    return `Unknown section '${section}' in ${basename}`;
}

function shape(item, label) {
    assertEqual(item.severity, 2, `${label} severity`);
    assertEqual(item.source, 'ADT-Syntax', `${label} source`);
    assertEqual(item.code, 'vertexadt/unknown-section', `${label} code`);
    assert(
        typeof item.message === 'string' && !item.message.includes(' has an error: '),
        `${label} unwrapped envelope`
    );
    assert(item.relatedInformation === undefined || item.relatedInformation === null, `${label} no related`);
}

async function openPull(session, relPath) {
    const filePath = path.join(CUSTOM, relPath);
    const text = fs.readFileSync(filePath, 'utf8');
    const uri = pathToFileURL(filePath).href;
    const lines = linesOf(text);
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    const pull = await session.diagnostic(uri);
    assert(pull && pull.kind === 'full', `${relPath} pull kind`);
    assert(pull.relatedDocuments === undefined, `${relPath} has no relatedDocuments`);
    return { uri, text, lines, items: unknownItems(pull.items) };
}

function expectWarning(items, lines, lineText, section, basename) {
    const line = lineIndex(lines, lineText);
    const found = items.filter((item) => item.range && item.range.start.line === line);
    assertEqual(found.length, 1, `${JSON.stringify(lineText)} warning count`);
    const item = found[0];
    shape(item, lineText);
    assertEqual(item.message, msg(section, basename), `${lineText} message`);
    assertEqual(item.range.start.line, line, `${lineText} start line`);
    assertEqual(item.range.end.line, line, `${lineText} end line`);
    assertEqual(underline(lines[line], item.range), section, `${lineText} underline`);
}

function expectSilent(items, lines, lineText) {
    const line = lineIndex(lines, lineText);
    const found = items.filter((item) => item.range && item.range.start.line === line);
    assertEqual(found.length, 0, `${JSON.stringify(lineText)} should be silent`);
}

async function run(session) {
    await session.initialize();

    const caller = await openPull(session, 'unknown_section_caller.prg');
    const calleeBase = 'unknown_section_callee.prg';
    const L = caller.lines;
    const I = caller.items;

    expectWarning(I, L, '# include unknown_section_callee.prg MACROX', 'MACROX', calleeBase);
    expectSilent(I, L, '# include unknown_section_callee.prg MACROS');
    expectSilent(I, L, '# include unknown_section_callee.prg MAC');
    expectWarning(I, L, '# include unknown_section_callee.prg JOINTS', 'JOINTS', calleeBase);
    expectSilent(I, L, '# include "unknown_section_callee.prg" JOINT');
    expectWarning(I, L, '# include "unknown_section_callee.prg BAD"', 'BAD', calleeBase);
    expectWarning(I, L, 'Ret$=call("unknown_section_callee.prg MACROX",Area)', 'MACROX', calleeBase);
    expectSilent(I, L, 'call("unknown_section_callee.prg", MACROS, Area)');
    expectSilent(I, L, 'call unknown_section_callee.prg JOINT');
    expectWarning(I, L, 'call unknown_section_callee.prg JOINTS', 'JOINTS', calleeBase);
    expectSilent(I, L, '# INCLUDE unknown_section_callee.prg MACROX');
    expectSilent(I, L, '##included_from unknown_section_callee.prg MACROX');
    expectSilent(I, L, '# include unknown_section_callee.prg SECTION');
    expectSilent(I, L, '# include unknown_section_callee.prg SECTIONOFFILE');
    expectSilent(I, L, '!# include unknown_section_callee.prg MACROX');
    expectSilent(I, L, '# include missing_no_file.prg MACROX');
    expectWarning(I, L, '# include unknown_section_plain.prg MACROS', 'MACROS', 'unknown_section_plain.prg');
    expectWarning(I, L, '# include unknown_section_empty.prg FOO', 'FOO', 'unknown_section_empty.prg');
    expectSilent(I, L, 'call("unknown_section_callee.prg MACROS")');
    expectWarning(I, L, `s$ = 'call("unknown_section_callee.prg BADNAME")'`, 'BADNAME', calleeBase);
    expectWarning(I, L, '##call("unknown_section_callee.prg BADNAME")', 'BADNAME', calleeBase);
    expectWarning(I, L, 'debug call("unknown_section_callee.prg BADNAME")', 'BADNAME', calleeBase);
    expectWarning(I, L, 'messagebox call("unknown_section_callee.prg BADNAME")', 'BADNAME', calleeBase);
    expectSilent(I, L, '# include unknown_section_dir MACROX');
    expectWarning(I, L, 'call "unknown_section_callee.prg JOINTS"', 'JOINTS', calleeBase);
    expectWarning(I, L, '# include "unknown_section_callee.prg JOINTS"', 'JOINTS', calleeBase);
    expectWarning(
        I,
        L,
        'call("a_missing.prg SEC1"); call("unknown_section_callee.prg SEC2")',
        'SEC2',
        calleeBase
    );
    expectWarning(I, L, " # include 'unknown_section_callee.prg' macros", 'macros', calleeBase);
    expectWarning(
        I,
        L,
        '# include "$DIR_MACROS/unknown_section_foo.prg MACROX"',
        'MACROX',
        'unknown_section_foo.prg'
    );
    expectSilent(I, L, 'call callee.prg SECTION');
    expectSilent(I, L, 'x=1;!call("unknown_section_callee.prg MACROX")');
    expectWarning(I, L, 'call("unknown_section_callee.prg MACROX");!bang', 'MACROX', calleeBase);

    assertEqual(I.length, 17, 'caller unknown-section count');

    const form = await openPull(session, 'unknown_section_edit_form.prg');
    const formIncludeLines = form.lines
        .map((text, line) => ({ text, line }))
        .filter((row) => row.text === '# include unknown_section_callee.prg MACROX');
    assertEqual(formIncludeLines.length, 2, 'edit-form include sites');
    const bodyLine = formIncludeLines[0].line;
    const afterLine = formIncludeLines[1].line;
    assertEqual(
        form.items.filter((item) => item.range.start.line === bodyLine).length,
        0,
        'EDIT_FORM body skipped'
    );
    const after = form.items.filter((item) => item.range.start.line === afterLine);
    assertEqual(after.length, 1, 'after EDIT_FORM warns');
    shape(after[0], 'edit-form after');
    assertEqual(after[0].message, msg('MACROX', calleeBase), 'edit-form after message');
    assertEqual(underline(form.lines[afterLine], after[0].range), 'MACROX', 'edit-form after underline');

    const calleePath = path.join(CUSTOM, 'unknown_section_callee.prg');
    session.didOpen({
        uri: pathToFileURL(calleePath).href,
        languageId: 'vertexadt',
        version: 1,
        text: 'x = 1\n',
    });
    const bufferCaller = await openPull(session, 'unknown_section_buffer_caller.prg');
    expectWarning(
        bufferCaller.items,
        bufferCaller.lines,
        '# include unknown_section_callee.prg MACROS',
        'MACROS',
        calleeBase
    );

    session.didChangeConfiguration({
        vertexadt: { diagnostics: { maxNumberOfProblems: 2 } },
    });
    const cap = await openPull(session, 'unknown_section_cap.prg');
    assertEqual(cap.items.length, 2, 'cap keeps two unknown-section diagnostics');
    expectWarning(cap.items, cap.lines, '# include unknown_section_callee.prg MACROX', 'MACROX', calleeBase);
    expectWarning(cap.items, cap.lines, '# include unknown_section_callee.prg JOINTS', 'JOINTS', calleeBase);
    expectSilent(cap.items, cap.lines, '# include unknown_section_callee.prg BAD1');
    expectSilent(cap.items, cap.lines, '# include unknown_section_callee.prg BAD2');
    expectSilent(cap.items, cap.lines, '# include unknown_section_callee.prg BAD3');

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'include-call-sections' }],
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
            console.log('lsp golden: 19-unknown-section passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
