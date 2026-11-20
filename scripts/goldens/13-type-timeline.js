#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'vector-to-value-type');
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

function itemsOf(pull) {
    return (pull && pull.items) || [];
}

function typeWarnings(items) {
    return items.filter((item) => typeof item.message === 'string' && item.message.includes('was previously typed as'));
}

function lineAt(text, index) {
    return text.split('\n')[index] ?? '';
}

function innerMessage(lineText, varName, vectorType, sectionName) {
    const sectionSuffix = sectionName ? ` in section ${sectionName}` : '';
    return `${lineText} has an error: Variable ${varName} was previously typed as ${vectorType}${sectionSuffix} and is now typed as value.`;
}

function assertWarning(diag, { text, varName, vectorType, sectionName, valueLine, vectorLine, label }) {
    assert(diag, `${label}: missing diagnostic`);
    const lineText = lineAt(text, valueLine);
    assertEqual(diag.severity, 2, `${label}: severity Warning`);
    assertEqual(diag.source, 'ADT-Syntax', `${label}: source`);
    assert(diag.code === undefined || diag.code === null, `${label}: no code`);
    assertEqual(diag.message, innerMessage(lineText, varName, vectorType, sectionName), `${label}: message`);
    assertEqual(diag.range && diag.range.start && diag.range.start.line, valueLine, `${label}: value line`);
    assertEqual(diag.range && diag.range.start && diag.range.start.character, 1, `${label}: range starts at character 1`);
    assertEqual(diag.range && diag.range.end && diag.range.end.line, valueLine, `${label}: end line`);
    assertEqual(diag.range && diag.range.end && diag.range.end.character, lineText.length, `${label}: end character`);
    const related = diag.relatedInformation || [];
    assertEqual(related.length, 1, `${label}: one relatedInformation`);
    assertEqual(related[0] && related[0].message, `First typed as ${vectorType} here`, `${label}: related message`);
    assertEqual(
        related[0] && related[0].location && related[0].location.range && related[0].location.range.start && related[0].location.range.start.line,
        vectorLine,
        `${label}: related line`
    );
}

async function pullFile(session, name) {
    const uri = uriFor(name);
    const text = readCustom(name);
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    const pull = await session.diagnostic(uri);
    assertEqual(pull && pull.kind, 'full', `${name}: pull kind`);
    return { uri, text, items: typeWarnings(itemsOf(pull)) };
}

async function run(session) {
    await session.initialize();

    const pvectorThenValue = await pullFile(session, '13-pvector-then-value.prg');
    assertEqual(pvectorThenValue.items.length, 1, 'pvector then value should warn once');
    assertWarning(pvectorThenValue.items[0], {
        text: pvectorThenValue.text,
        varName: 'MyP',
        vectorType: 'pvector',
        valueLine: 1,
        vectorLine: 0,
        label: 'pvector-then-value',
    });

    const vectorOnly = await pullFile(session, '13-vector-only.prg');
    assertEqual(vectorOnly.items.length, 0, 'vector only should not warn');

    const valueThenVector = await pullFile(session, '13-value-then-vector.prg');
    assertEqual(valueThenVector.items.length, 0, 'value before vector should not warn');

    const vectorVectorValue = await pullFile(session, '13-vector-vector-value.prg');
    assertEqual(vectorVectorValue.items.length, 1, 'pvector to dvector to value should warn on value line');
    assertWarning(vectorVectorValue.items[0], {
        text: vectorVectorValue.text,
        varName: 'MyP',
        vectorType: 'pvector',
        sectionName: 'MACROS',
        valueLine: 3,
        vectorLine: 1,
        label: 'vector-vector-value',
    });

    const valueVectorValue = await pullFile(session, '13-value-vector-value.prg');
    assertEqual(valueVectorValue.items.length, 1, 'value after vector should warn even if value appeared earlier');
    assertWarning(valueVectorValue.items[0], {
        text: valueVectorValue.text,
        varName: 'MyP',
        vectorType: 'pvector',
        sectionName: 'MACROS',
        valueLine: 3,
        vectorLine: 2,
        label: 'value-vector-value',
    });

    const arrayThenValue = await pullFile(session, '13-array-then-value.prg');
    assertEqual(arrayThenValue.items.length, 0, 'array to value should not warn');

    const differentSections = await pullFile(session, '13-different-sections.prg');
    assertEqual(differentSections.items.length, 0, 'vector in one section and value in another should not warn');

    const bothSections = await pullFile(session, '13-same-name-both-sections.prg');
    assertEqual(bothSections.items.length, 2, 'each section should warn independently');
    assertWarning(bothSections.items[0], {
        text: bothSections.text,
        varName: 'MyP',
        vectorType: 'pvector',
        sectionName: 'MACROS',
        valueLine: 2,
        vectorLine: 1,
        label: 'both-sections-macros',
    });
    assertWarning(bothSections.items[1], {
        text: bothSections.text,
        varName: 'MyP',
        vectorType: 'pvector',
        sectionName: 'MACROS2',
        valueLine: 6,
        vectorLine: 5,
        label: 'both-sections-macros2',
    });

    const ignoreType = await pullFile(session, '13-ignore-type.prg');
    assertEqual(ignoreType.items.length, 0, 'IGNORE TYPE skips the vector write');

    const commentType = await pullFile(session, '13-comment-type.prg');
    assertEqual(commentType.items.length, 1, '[pvector] comment fallback then value should warn');
    assertWarning(commentType.items[0], {
        text: commentType.text,
        varName: 'Foo',
        vectorType: 'pvector',
        valueLine: 1,
        vectorLine: 0,
        label: 'comment-type',
    });

    const editForm = await pullFile(session, '13-edit-form.prg');
    assertEqual(editForm.items.length, 0, '#EDIT_FORM# bodies skipped at compare');

    const includeMain = await pullFile(session, '13-include-main.prg');
    assertEqual(includeMain.items.length, 1, 'included_from first type then value should warn');
    assertWarning(includeMain.items[0], {
        text: includeMain.text,
        varName: 'IncP',
        vectorType: 'pvector',
        valueLine: 1,
        vectorLine: 0,
        label: 'include-main',
    });

    const ignoreInclude = await pullFile(session, '13-ignore-include.prg');
    assertEqual(ignoreInclude.items.length, 0, 'IGNORE TYPE on include skips handleIncludedFile');

    const vecTo = await pullFile(session, '13-vec-to-first-vector.prg');
    assertEqual(vecTo.items.length, 1, 'vec_to input pvector wins over same-line xvector');
    assertWarning(vecTo.items[0], {
        text: vecTo.text,
        varName: 'MyP',
        vectorType: 'pvector',
        valueLine: 1,
        vectorLine: 0,
        label: 'vec-to-first-vector',
    });

    const copyAssign = await pullFile(session, '13-copy-assign.prg');
    assertEqual(copyAssign.items.length, 1, 'Name = Other copy of vector then value should warn');
    assertWarning(copyAssign.items[0], {
        text: copyAssign.text,
        varName: 'DstP',
        vectorType: 'pvector',
        valueLine: 2,
        vectorLine: 1,
        label: 'copy-assign',
    });

    const incUri = uriFor('macros/13-inc.prg');
    session.didOpen({
        uri: incUri,
        languageId: 'vertexadt',
        version: 1,
        text: 'IncP = 1 ;!"[value]"\n',
    });
    const bufferMain = await pullFile(session, '13-include-buffer.prg');
    assertEqual(bufferMain.items.length, 0, 'buffer-wins included snapshot: value comment does not warn');

    session.didChangeConfiguration({
        variables: { warnVectorToValueTransition: false },
        warnVectorToValueTransition: false,
    });
    const settingUri = uriFor('13-pvector-then-value.prg');
    session.didChange({
        uri: settingUri,
        version: 2,
        contentChanges: [{ text: readCustom('13-pvector-then-value.prg') }],
    });
    const offPull = await session.diagnostic(settingUri);
    assertEqual(typeWarnings(itemsOf(offPull)).length, 0, 'setting off skips the compare');

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'vector-to-value-type' }],
    });
    try {
        await run(session);
    } catch (err) {
        session.kill();
        throw err;
    }
    console.log('lsp golden: 13-type-timeline passed');
}

if (require.main === module) {
    main().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run: main };
