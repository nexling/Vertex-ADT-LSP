'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE_ROOT = path.join(ROOT, 'test-fixtures', 'local-slot-names');
const CUSTOM = path.join(FIXTURE_ROOT, 'custom');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function byCode(items, code) {
    return (items || []).filter((item) => item.code === code);
}

function dump(items) {
    return JSON.stringify(items, null, 2);
}

function openFixture(session, name) {
    const filePath = path.join(CUSTOM, name);
    const uri = pathToFileURL(filePath).href;
    const text = fs.readFileSync(filePath, 'utf8');
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    return { uri, text, filePath };
}

async function main() {
    const fixtureRootUri = pathToFileURL(FIXTURE_ROOT).href;
    const session = LspSession.launch({
        rootUri: fixtureRootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureRootUri, name: 'local-slot-names' }],
    });

    try {
        await session.initialize();
        session.didChangeConfiguration({
            vertexadt: {
                strictMode: { enabled: true },
                locals: {
                    validateUniqueNames: true,
                    requireLNamesInCallees: true,
                },
            },
        });

        const files = fs.readdirSync(CUSTOM).filter((name) => name.endsWith('.prg'));
        const opened = {};
        for (const name of files) {
            opened[name] = openFixture(session, name);
        }
        await session.waitQuiet(400);
        await session.hover(opened['callee_a.prg'].uri, 0, 0);

        const pull = async (name) => {
            const result = await session.diagnostic(opened[name].uri);
            assert(result && result.kind === 'full', `${name} pull kind`);
            return result.items || [];
        };

        const calleeA = await pull('callee_a.prg');
        const calleeB = await pull('callee_b.prg');
        const calleeC = await pull('callee_c.prg');
        const calleeBad = await pull('callee_bad_names.prg');
        const manifestOk = await pull('callee_manifest_ok.prg');
        const manifestBad = await pull('callee_manifest_bad.prg');
        const unusedL17 = await pull('callee_l17_unused.prg');

        const collisionA = byCode(calleeA, 'vertexadt/local-slot-name-collision');
        const collisionB = byCode(calleeB, 'vertexadt/local-slot-name-collision');
        const collisionC = byCode(calleeC, 'vertexadt/local-slot-name-collision');
        assert(
            collisionA.length + collisionB.length === 1,
            `Scenario 1: exactly one of callee_a/callee_b reports the l1 collision\nA=${dump(collisionA)}\nB=${dump(collisionB)}`
        );
        const collision = collisionA.length ? collisionA[0] : collisionB[0];
        const ownerName = collisionA.length ? 'callee_b.prg' : 'callee_a.prg';
        const laterName = collisionA.length ? 'callee_a.prg' : 'callee_b.prg';
        assert(collision.severity === 3, `Scenario 1 ${laterName} severity Information`);
        assert(collision.source === 'ADT-Syntax', 'Scenario 1 source');
        assert(
            typeof collision.message === 'string'
                && collision.message.includes('Local slot prefix l1 is already used by callee ')
                && collision.message.includes(ownerName),
            `Scenario 1 message names owner ${ownerName}: ${collision.message}`
        );
        assert(
            Array.isArray(collision.relatedInformation) && collision.relatedInformation.length === 1,
            'Scenario 1 relatedInformation'
        );
        assert(
            collision.relatedInformation[0].message.includes(`(l1) in ${ownerName}`),
            `Scenario 1 related: ${JSON.stringify(collision.relatedInformation[0].message)}`
        );

        assert(collisionC.length === 0, `Scenario 2: l2 does not collide\n${dump(calleeC)}`);

        const naming = byCode(calleeBad, 'vertexadt/callee-local-naming');
        assert(naming.length === 1, `Scenario 4: one naming warning on foo\n${dump(calleeBad)}`);
        assert(naming[0].severity === 2, 'Scenario 4 severity Warning');
        assert(naming[0].source === 'ADT-Syntax', 'Scenario 4 source');
        assert(
            naming[0].message === 'Callee locals in this file should share prefix l19 (e.g. l19foo)',
            `Scenario 4 message: ${naming[0].message}`
        );
        const fooLine = opened['callee_bad_names.prg'].text.split(/\r?\n/)[1];
        const fooCol = fooLine.indexOf('foo');
        assert(naming[0].range && naming[0].range.start.line === 1, 'Scenario 4 range line is foo = 5');
        assert(naming[0].range.start.character === fooCol, 'Scenario 4 range starts at foo token');
        assert(naming[0].range.end.character === fooCol + 3, 'Scenario 4 range is the foo token');
        assert(
            !calleeBad.some((item) => item.range && item.range.start.line === 3 && item.code === 'vertexadt/callee-local-naming'),
            'Scenario 4 does not warn on set(foo, 1)'
        );

        assert(
            byCode(unusedL17, 'vertexadt/callee-local-naming').length === 0,
            'uncalled l17 program is not a callee and does not warn'
        );

        assert(
            byCode(manifestOk, 'vertexadt/local-slots-manifest').length === 0,
            `Scenario 5 ok has no manifest errors\n${dump(manifestOk)}`
        );

        const manifestIssues = byCode(manifestBad, 'vertexadt/local-slots-manifest');
        assert(manifestIssues.length === 2, `Scenario 5 bad has missing l1B then extra l3C\n${dump(manifestIssues)}`);
        assert(manifestIssues[0].severity === 1, 'Scenario 5 severity Error');
        assert(manifestIssues[0].source === 'ADT-Syntax', 'Scenario 5 source');
        assert(
            manifestIssues[0].message === '@local_slots lists l1B but no declaration for l1B was found',
            `Scenario 5 missing declaration: ${manifestIssues[0].message}`
        );
        assert(manifestIssues[0].range.start.line === 0, 'Scenario 5 missing-decl is the @local_slots line');
        assert(manifestIssues[0].range.start.character === 0, 'Scenario 5 missing-decl whole line');
        assert(
            manifestIssues[1].message === 'Declared local slot l3C is missing from @local_slots manifest',
            `Scenario 5 extra declaration: ${manifestIssues[1].message}`
        );
        assert(manifestIssues[1].range.start.line === 3, 'Scenario 5 extra is l3C = 3');
        assert(manifestIssues[1].range.start.character === 1, 'Scenario 5 extra uses offset+1');

        assert(
            byCode(calleeA, 'vertexadt/local-slots-manifest').length === 0,
            'callee_a has no @local_slots line so no manifest errors'
        );
        assert(
            byCode(manifestBad, 'vertexadt/callee-local-naming').length === 0,
            'manifest-bad is not a call target so family 16 is silent'
        );

        await session.shutdownAndExit();
        console.log('22-local-slots goldens passed');
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
