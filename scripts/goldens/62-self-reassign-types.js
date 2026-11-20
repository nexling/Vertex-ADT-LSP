#!/usr/bin/env node
'use strict';

// Self-reassignment goldens: test-fixtures/self-reassign-types/TESTING.md.
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'self-reassign-types');
const CUSTOM = path.join(FIXTURE, 'custom');

// Indices into the semantic-token legend (scripts/goldens/30-semantic-tokens.js).
const SELEM = 3;
const AREA = 8;
const VALUE = 14;
const PVECTOR = 17;
const DVECTOR = 25;
const PROPERTY_KIND = 10;

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

function decode(result) {
    const data = (result && result.data) || [];
    const tokens = [];
    let line = 0;
    let start = 0;
    for (let i = 0; i + 4 < data.length; i += 5) {
        line += data[i];
        start = data[i] === 0 ? start + data[i + 1] : data[i + 1];
        tokens.push({ line, start, type: data[i + 3] });
    }
    return tokens;
}

async function runFile(session) {
    const filePath = path.join(CUSTOM, 'self_reassign.prg');
    const uri = pathToFileURL(filePath).href;
    const text = fs.readFileSync(filePath, 'utf8');
    const lines = text.split(/\r?\n/);
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });

    const lineOf = (lineText) => {
        const i = lines.indexOf(lineText);
        assert(i >= 0, `missing fixture line ${JSON.stringify(lineText)}`);
        return i;
    };

    const pull = await session.diagnostic(uri);
    const items = (pull && pull.items) || [];
    const strict = items
        .filter((d) => d.message.includes('should be of type'))
        .map((d) => [d.range.start.line, d.message]);
    assertEqual(strict, [
        [
            lineOf('num = get_panel_core(num)'),
            "get_panel_core: input object 'num' should be of type [selemobj, area, wall, slope] but is 'value'",
        ],
        [
            lineOf('use = get_panel_core(other)'),
            "get_panel_core: input object 'other' should be of type [selemobj, area, wall, slope] but is 'value'",
        ],
    ], 'strict errors read the type before the line');

    const vecLine = lineOf('vec = vec_get_x(vec)');
    assert(
        items.some((d) => d.range.start.line === vecLine && d.message.includes('was previously typed as pvector')),
        'vector-to-value warning stays on self-reassignment'
    );
    const freshLine = lineOf('fresh = get_panel_core(fresh)');
    assert(
        items.some((d) => d.range.start.line === freshLine && d.message.includes('Variable fresh used before assignment')),
        'untyped right-hand side is used before assignment'
    );

    const tokens = decode(await session.semanticTokensFull(uri));
    const typeAt = (lineText, word, occurrence) => {
        const line = lineOf(lineText);
        const words = [...lineText.matchAll(new RegExp(`\\b${word}\\b`, 'g'))];
        assert(words.length >= occurrence, `missing ${word} #${occurrence} on ${lineText}`);
        const at = words[occurrence - 1].index;
        const hit = tokens.find((t) => t.line === line && t.start === at);
        return hit ? hit.type : null;
    };
    const cases = [
        ['obj = get_panel_core(obj)', 'obj', 1, AREA, 'left obj takes the return type'],
        ['obj = get_panel_core(obj)', 'obj', 2, SELEM, 'right obj keeps the earlier type'],
        ['use = get_panel_core(obj)', 'obj', 1, AREA, 'next line sees the return type'],
        ['num = get_panel_core(num)', 'num', 1, AREA, 'left num'],
        ['num = get_panel_core(num)', 'num', 2, VALUE, 'right num'],
        ['vec_add(dv) dv, pt', 'dv', 1, PVECTOR, 'output slot dv'],
        ['vec_add(dv) dv, pt', 'dv', 2, DVECTOR, 'input dv of the same name'],
        ['vec = vec_get_x(vec)', 'vec', 1, VALUE, 'left vec'],
        ['vec = vec_get_x(vec)', 'vec', 2, PVECTOR, 'right vec'],
        ['fresh = get_panel_core(fresh)', 'fresh', 1, AREA, 'left fresh'],
        ['fresh = get_panel_core(fresh)', 'fresh', 2, null, 'right fresh is untyped'],
    ];
    for (const [lineText, word, occurrence, expected, label] of cases) {
        assertEqual(typeAt(lineText, word, occurrence), expected, label);
    }
}

async function runCompletion(session) {
    const uri = pathToFileURL(path.join(CUSTOM, 'completion_probe.prg')).href;
    const text = 'P = new_piece(1)\nP = get_obj_param(P, ';
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    const result = await session.request('textDocument/completion', {
        textDocument: { uri },
        position: { line: 1, character: text.split('\n')[1].length },
        context: { triggerKind: 1 },
    });
    const labels = result.items
        .filter((item) => item.kind === PROPERTY_KIND && typeof item.sortText === 'string' && item.sortText.startsWith('0'))
        .map((item) => item.label);
    assert(labels.includes('FPTYPE'), `right P completes as a profile, got ${JSON.stringify(labels)}`);
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'self-reassign-types' }],
    });
    try {
        await session.initialize();
        session.didChangeConfiguration({ strictMode: { enabled: true } });
        await runFile(session);
        await runCompletion(session);
        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
    console.log('lsp golden: 62-self-reassign-types passed');
}

if (require.main === module) {
    main().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run: main };
