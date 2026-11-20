'use strict';

/**
 * Parity goldens for unused-variable diagnostics (family 6).
 * Assertions follow test-fixtures/unused-variables/TESTING.md and unused
 * suppression in test-fixtures/verification-ignored-sections/TESTING.md.
 *
 *   node scripts/goldens/17-unused-variables.js
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'unused-variables');
const IGNORE_FIXTURE = path.join(ROOT, 'test-fixtures', 'verification-ignored-sections');

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
    if (actualJson !== expectedJson) {
        throw new AssertionError(`${message}: expected ${expectedJson}, got ${actualJson}`);
    }
}

function readFixture(dir, rel) {
    return fs.readFileSync(path.join(dir, rel), 'utf8');
}

function unusedDiagnostics(items) {
    return (items || []).filter((item) => {
        const message = item && item.message;
        return typeof message === 'string' && /Variable .+ is declared but not used\./.test(message);
    });
}

function unusedNames(items) {
    return unusedDiagnostics(items).map((item) => {
        const match = /Variable (.+) is declared but not used\./.exec(item.message);
        return match ? match[1] : item.message;
    });
}

function lineTextAt(text, line) {
    return text.split('\n')[line] || '';
}

function expectedWrap(lineText, varName) {
    return `${lineText} has an error: Variable ${varName} is declared but not used.`;
}

async function openAndPull(session, dir, rel, version = 1) {
    const abs = path.join(dir, rel);
    const uri = pathToFileURL(abs).href;
    const text = fs.readFileSync(abs, 'utf8');
    session.didOpen({ uri, languageId: 'vertexadt', version, text });
    return { uri, text };
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'unused-variables' }],
    });
    try {
        await session.initialize();

        const localUnusedPath = 'custom/local_unused.prg';
        const offOpen = await openAndPull(session, FIXTURE, localUnusedPath);
        await session.waitQuiet();
        const offPull = await session.diagnostic(offOpen.uri);
        assertEqual(offPull && offPull.kind, 'full', 'gate-off pull kind');
        assertEqual(
            unusedNames(offPull && offPull.items),
            [],
            'noteUnusedVariables default off emits no unused diagnostics'
        );
        session.didClose(offOpen.uri);

        session.didChangeConfiguration({
            variables: { noteUnusedVariables: true },
        });
        await session.waitQuiet();

        const files = [
            'custom/local_unused.prg',
            'custom/local_used.prg',
            'custom/caller_passes_arg.prg',
            'custom/callee_uses_arg.prg',
            'custom/caller_unused_arg.prg',
            'custom/callee_no_arg.prg',
            'custom/include_uses_var.prg',
            'custom/included_body.prg',
            'custom/section_isolated.prg',
            'custom/if_condition_use.prg',
            'custom/if_condition_spaced.prg',
            'custom/if_condition_multiline.prg',
            'custom/if_condition_single_eq.prg',
        ];
        const opened = {};
        for (const rel of files) {
            opened[rel] = await openAndPull(session, FIXTURE, rel);
        }
        await session.waitQuiet(500);

        const pulls = {};
        for (const rel of files) {
            pulls[rel] = await session.diagnostic(opened[rel].uri, 15000);
            assertEqual(pulls[rel] && pulls[rel].kind, 'full', `${rel} pull kind`);
        }

        const localUnusedItems = unusedDiagnostics(pulls[localUnusedPath].items);
        assertEqual(unusedNames(localUnusedItems).sort(), ['bar', 'foo'], 'local_unused flags foo and bar');
        for (const item of localUnusedItems) {
            const match = /Variable (.+) is declared but not used\./.exec(item.message);
            const varName = match[1];
            const line = item.range.start.line;
            const lineText = lineTextAt(opened[localUnusedPath].text, line);
            assertEqual(item.source, 'ADT-Syntax', `local_unused ${varName} source`);
            assertEqual(item.severity, 3, `local_unused ${varName} Information`);
            assertEqual(item.message, expectedWrap(lineText, varName), `local_unused ${varName} envelope`);
            assertEqual(item.range.start.character, 1, `local_unused ${varName} range starts at offset+1`);
            assert(item.code === undefined || item.code === null, `local_unused ${varName} has no code`);
        }

        const localUsedNames = unusedNames(pulls['custom/local_used.prg'].items);
        assert(localUsedNames.includes('bar'), 'local_used flags bar');
        assert(!localUsedNames.includes('foo'), 'local_used does not flag foo');

        const callerPassesNames = unusedNames(pulls['custom/caller_passes_arg.prg'].items);
        assert(!callerPassesNames.includes('arg1'), `caller_passes_arg does not flag arg1; got ${callerPassesNames.join(',')}`);

        const callerUnusedNames = unusedNames(pulls['custom/caller_unused_arg.prg'].items);
        assert(callerUnusedNames.includes('arg1'), `caller_unused_arg flags arg1; got ${callerUnusedNames.join(',')}`);

        const includeNames = unusedNames(pulls['custom/include_uses_var.prg'].items);
        assertEqual(includeNames, [], 'include_uses_var has 0 unused on the parent');

        const sectionItems = unusedDiagnostics(pulls['custom/section_isolated.prg'].items);
        const sectionText = opened['custom/section_isolated.prg'].text;
        const unusedDupMacros2 = sectionItems.find((item) => {
            const name = /Variable (.+) is declared but not used\./.exec(item.message);
            return name && name[1] === 'dup' && /#MACROS2#/.test(sectionText.split('\n').slice(0, item.range.start.line + 1).join('\n'));
        });
        const dupInMacros = sectionItems.filter((item) => {
            const name = /Variable (.+) is declared but not used\./.exec(item.message);
            if (!name || name[1] !== 'dup') {
                return false;
            }
            const prefix = sectionText.split('\n').slice(0, item.range.start.line + 1).join('\n');
            return /#MACROS#/.test(prefix) && !/#MACROS2#/.test(prefix);
        });
        assert(unusedDupMacros2, 'section_isolated flags dup in MACROS2');
        assertEqual(dupInMacros.length, 0, 'section_isolated does not flag dup in MACROS');
        assert(
            unusedNames(sectionItems).includes('used'),
            'section_isolated flags used (declared in MACROS, never referenced)'
        );

        assert(
            !unusedNames(pulls['custom/if_condition_use.prg'].items).includes('I'),
            'if_condition_use does not flag I'
        );
        assert(
            !unusedNames(pulls['custom/if_condition_spaced.prg'].items).includes('I'),
            'if_condition_spaced does not flag I'
        );
        assert(
            !unusedNames(pulls['custom/if_condition_multiline.prg'].items).includes('I'),
            'if_condition_multiline does not flag I'
        );
        assert(
            !unusedNames(pulls['custom/if_condition_single_eq.prg'].items).includes('flatblock'),
            'if_condition_single_eq does not flag flatblock'
        );

        const ignoreRel = 'custom/edit_form_ignored.prg';
        const ignoreOpen = await openAndPull(session, IGNORE_FIXTURE, ignoreRel);
        await session.waitQuiet();
        const ignorePull = await session.diagnostic(ignoreOpen.uri, 15000);
        const ignoreUnused = unusedDiagnostics(ignorePull.items);
        assert(
            !unusedNames(ignoreUnused).includes('UNUSED'),
            `EDIT_FORM body does not flag UNUSED; got ${unusedNames(ignoreUnused).join(',')}`
        );
        const formBodyLines = new Set();
        {
            const lines = ignoreOpen.text.split('\n');
            let open = -1;
            for (let i = 0; i < lines.length; i++) {
                if (lines[i].trim() === '#EDIT_FORM#') {
                    if (open === -1) {
                        open = i;
                    } else {
                        for (let n = open + 1; n < i; n++) {
                            formBodyLines.add(n);
                        }
                        open = -1;
                    }
                }
            }
        }
        const insideForm = ignorePull.items.filter((item) => formBodyLines.has(item.range.start.line));
        assertEqual(
            insideForm.length,
            0,
            `no diagnostics inside #EDIT_FORM# body; got ${JSON.stringify(insideForm)}`
        );

        await session.shutdownAndExit();
        console.log('17-unused-variables goldens passed');
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
