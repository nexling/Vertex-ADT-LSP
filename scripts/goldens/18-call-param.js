#!/usr/bin/env node
'use strict';

/**
 * Parity goldens for family 7 call-param count check.
 * Assertions from test-fixtures/call-param-validation/TESTING.md plus the
 * $CALLPARAM expected-count cases from include-call-sections.
 *
 *   node scripts/goldens/18-call-param.js
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '..', '..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'call-param-validation');
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

function callParenRange(text, lineIndex) {
    const line = text.split(/\n/)[lineIndex].replace(/\r$/, '');
    const match = /\bcall\s*\(/i.exec(line);
    assert(match, `call( on line ${lineIndex} of fixture`);
    const open = match.index + match[0].length - 1;
    let depth = 1;
    let inQuote = false;
    for (let i = open + 1; i < line.length; i++) {
        const ch = line[i];
        if (inQuote) {
            if (ch === '"') {
                inQuote = false;
            }
            continue;
        }
        if (ch === '"') {
            inQuote = true;
            continue;
        }
        if (ch === '(') {
            depth++;
        } else if (ch === ')') {
            depth--;
            if (depth === 0) {
                return {
                    start: { line: lineIndex, character: open },
                    end: { line: lineIndex, character: i + 1 },
                };
            }
        }
    }
    throw new AssertionError(`unclosed call( on line ${lineIndex}`);
}

function isCallParam(item) {
    return item && (
        item.code === 'vertexadt/missing-call-params'
        || item.code === 'vertexadt/extra-call-params'
    );
}

function summarize(items) {
    return (items || []).filter(isCallParam).map((item) => ({
        message: item.message,
        severity: item.severity,
        source: item.source,
        code: item.code,
        line: item.range && item.range.start && item.range.start.line,
        range: item.range,
    }));
}

async function openCustom(session, name, version = 1) {
    const text = readCustom(name);
    const uri = uriFor(name);
    session.didOpen({ uri, languageId: 'vertexadt', version, text });
    return { uri, text };
}

async function main() {
    const rootUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'call-param-validation' }],
    });

    try {
        await session.initialize();

        await openCustom(session, 'callee_three_params.prg');
        await openCustom(session, 'sectional_callee.prg');
        await openCustom(session, 'callee_no_params.prg');
        await session.waitQuiet();

        const missing = await openCustom(session, 'caller_missing_one.prg');
        const missingPull = await session.diagnostic(missing.uri);
        assertEqual(missingPull && missingPull.kind, 'full', 'missing pull kind');
        const missingItems = summarize(missingPull.items);
        assertEqual(missingItems.length, 1, 'TESTING.md scenario 1: one missing diagnostic');
        assertEqual(missingItems[0].severity, 1, 'missing is Error');
        assertEqual(missingItems[0].source, 'ADT-Syntax', 'missing source');
        assertEqual(missingItems[0].code, 'vertexadt/missing-call-params', 'missing code');
        assertEqual(
            missingItems[0].message,
            'call expects 3 parameters but 2 were provided',
            'TESTING.md scenario 1 message'
        );
        assertEqual(missingItems[0].range, callParenRange(missing.text, 0), 'missing range is (...)');

        const ok = await openCustom(session, 'caller_ok.prg');
        const okPull = await session.diagnostic(ok.uri);
        assertEqual(summarize(okPull.items), [], 'TESTING.md scenario 2: no call-param diagnostic');

        const extra = await openCustom(session, 'caller_extra_one.prg');
        const extraPull = await session.diagnostic(extra.uri);
        const extraItems = summarize(extraPull.items);
        assertEqual(extraItems.length, 1, 'TESTING.md scenario 3: one extra diagnostic');
        assertEqual(extraItems[0].severity, 2, 'extra is Warning');
        assertEqual(extraItems[0].source, 'ADT-Syntax', 'extra source');
        assertEqual(extraItems[0].code, 'vertexadt/extra-call-params', 'extra code');
        assertEqual(
            extraItems[0].message,
            'call expects 3 parameters but 4 were provided',
            'TESTING.md scenario 3 message'
        );
        assertEqual(extraItems[0].range, callParenRange(extra.text, 0), 'extra range is (...)');

        // include-call-sections $CALLPARAM asserts (ticket 20 integer, ticket 22 skip/compare).
        const sectional = await openCustom(session, 'caller_sectional.prg');
        const sectionalPull = await session.diagnostic(sectional.uri);
        const sectionalItems = summarize(sectionalPull.items);
        assertEqual(
            sectionalItems.map((item) => ({
                line: item.line,
                code: item.code,
                message: item.message,
                severity: item.severity,
            })),
            [
                {
                    line: 1,
                    code: 'vertexadt/missing-call-params',
                    message: 'call expects 2 parameters but 1 were provided',
                    severity: 1,
                },
                {
                    line: 4,
                    code: 'vertexadt/extra-call-params',
                    message: 'call expects 2 parameters but 3 were provided',
                    severity: 2,
                },
                {
                    line: 5,
                    code: 'vertexadt/missing-call-params',
                    message: 'call expects 2 parameters but 1 were provided',
                    severity: 1,
                },
            ],
            'include-call-sections $CALLPARAM: SECTION1=0 skip, OTHER=2, whole-file=2'
        );

        const skips = await openCustom(session, 'caller_skips.prg');
        const skipPull = await session.diagnostic(skips.uri);
        const skipItems = summarize(skipPull.items);
        assertEqual(
            skipItems.map((item) => ({ line: item.line, code: item.code })),
            [
                { line: 0, code: 'vertexadt/missing-call-params' },
                { line: 6, code: 'vertexadt/missing-call-params' },
            ],
            'caller bang / EDIT_FORM / space-form / missing file / expected<=0 / recall skipped'
        );

        session.didChangeConfiguration({
            calls: { validateParameterCount: false },
        });
        const gated = await session.diagnostic(missing.uri);
        assertEqual(summarize(gated.items), [], 'validateParameterCount false: no diagnostics');

        await session.shutdownAndExit();
        console.log('lsp golden: 18-call-param passed');
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}

module.exports = { run: main };
