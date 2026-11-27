#!/usr/bin/env node
'use strict';

/**
 * Typed iterator items (docs/adr/0030-typed-iterator-items.md):
 * test-fixtures/typed-iterators/TESTING.md. `next_item` takes its type from
 * the iterator's first value (or `joint` for a CONNECTIONS task); a
 * CONNECTIONS iterator with a first value other than "" is an error, an
 * unknown first value is information.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'typed-iterators');
const CUSTOM = path.join(FIXTURE, 'custom');
const CODES = new Set(['vertexadt/iterator-connections-type', 'vertexadt/iterator-unknown-type']);

// Semantic-token legend indices (server/src/semantic_tokens.cpp).
const T = {
    edge: 2, selemobj: 3, profileend: 4, profile: 6, sheet: 7, area: 8, joint: 9,
    slope: 11, wall: 15, macro: 26,
};

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const a = JSON.stringify(actual);
    const e = JSON.stringify(expected);
    assert(a === e, `${message}: expected ${e}, got ${a}`);
}

function messageOf(item) {
    return typeof item.message === 'string' ? item.message : (item.message && item.message.value) || '';
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

// [line prefix, literal, severity (1 error, 3 information), message]
const DIAGNOSTICS = [
    ['ItHc = ', '"PROFOBJ"', 1, 'CONNECTIONS_HC iterators return joints; first value "PROFOBJ" returns profile'],
    ['ItBad = ', '"FOO"', 1, 'CONNECTIONS iterators return joints; first value must be "", not "FOO"'],
    ['ItTypo = ', '"MACROOBJ"', 3,
        'Unknown iterator object type "MACROOBJ"; will return all objects (did you mean "MACROBJ"?)'],
    ['ItTypo2 = ', '"MACOBJ"', 3,
        'Unknown iterator object type "MACOBJ"; will return all objects (did you mean "MACROBJ"?)'],
    ['ItCase = ', '"profobj"', 3,
        'Unknown iterator object type "profobj"; will return all objects (did you mean "PROFOBJ"?)'],
    ['ItPiece = ', '"PIECE"', 3, 'Unknown iterator object type "PIECE"; will return all objects'],
    ['init_iterator(ItInitBad)', '"AREAOBJ"', 1, 'CONNECTIONS iterators return joints; first value "AREAOBJ" returns area'],
];

// [next_item target, expected type (null: untyped)]
const ITEMS = [
    ['Prof', T.profile], ['Area', T.area], ['Wall', T.wall], ['Macro', T.macro], ['Sheet', T.sheet],
    ['PEnd', T.profileend], ['Edge', T.edge], ['Slope', T.slope], ['Truss', T.slope],
    ['Selem', T.selemobj], ['Notch', T.macro], ['LNotch', T.macro], ['ANotch', T.macro],
    ['Cut', T.macro], ['Dimple', T.macro], ['Feat', T.macro], ['Flange', T.macro],
    ['Joint', T.joint], ['JointHc', T.joint], ['JointBad', T.joint], ['JointKind', T.joint],
    ['Re1', T.profile], ['Re2', T.wall],
    ['Typo', null], ['Case1', null], ['Piece', null], ['All', null], ['Var1', null],
    ['Low', T.profile], ['JointInit', T.joint],
];

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'typed-iterators' }],
    });
    try {
        await session.initialize();
        session.didChangeConfiguration({ diagnostics: { maxNumberOfProblems: 100000 } });
        await session.waitQuiet(200);

        const filePath = path.join(CUSTOM, 'typed_iterators.prg');
        const uri = pathToFileURL(filePath).href;
        const text = fs.readFileSync(filePath, 'utf8');
        const lines = text.split(/\r?\n/);
        session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });

        const lineStarting = (prefix) => {
            const i = lines.findIndex((l) => l.trimStart().startsWith(prefix));
            assert(i >= 0, `missing fixture line starting ${JSON.stringify(prefix)}`);
            return i;
        };

        const report = await session.diagnostic(uri);
        const got = (report.items || [])
            .filter((d) => CODES.has(d.code))
            .map((d) => [d.range.start.line, d.range.start.character, d.range.end.character, d.severity, messageOf(d)]);
        const want = DIAGNOSTICS.map(([prefix, literal, severity, message]) => {
            const line = lineStarting(prefix);
            const start = lines[line].indexOf(literal);
            return [line, start, start + literal.length, severity, message];
        });
        assertEqual(got, want, 'iterator diagnostics');
        assert((report.items || []).filter((d) => CODES.has(d.code)).every((d) => d.source === 'ADT-Syntax'), 'source');

        const tokens = decode(await session.semanticTokensFull(uri));
        for (const [name, expected] of ITEMS) {
            const line = lineStarting(`${name} = next_item(`);
            const start = lines[line].indexOf(name);
            const hit = tokens.find((t) => t.line === line && t.start === start);
            const type = hit && hit.type !== 0 ? hit.type : null;
            assertEqual(type, expected, `${name} type`);
        }

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

module.exports = { run: main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 66-typed-iterators passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
