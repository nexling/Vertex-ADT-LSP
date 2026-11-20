#!/usr/bin/env node
'use strict';

// Program move (docs/adr/0017-native-program-move.md).
// Fixture: test-fixtures/program-move (see its TESTING.md). Nothing is
// written: the command and willRenameFiles only return edits.

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'program-move');
const CUSTOM = path.join(FIXTURE, 'custom');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    if (actualJson !== expectedJson) {
        throw new Error(`${message}: expected ${expectedJson}, got ${actualJson}`);
    }
}

function uriOf(...parts) {
    return pathToFileURL(path.join(FIXTURE, ...parts)).href;
}

function sameUri(a, b) {
    return decodeURIComponent(a).toLowerCase() === decodeURIComponent(b).toLowerCase();
}

function move(session, args) {
    return session.request('workspace/executeCommand', { command: 'vertexadt.moveProgram', arguments: [args] });
}

function editsFor(documentChanges, uri) {
    const change = documentChanges.find((item) => item.textDocument && sameUri(item.textDocument.uri, uri));
    return change ? change.edits.map((edit) => edit.newText) : [];
}

async function run() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'program-move' }],
    });
    await session.initialize();

    // 1. Capabilities: willRename on every file op, refactor.move actions.
    const caps = session.initializeResult.capabilities;
    assertEqual(
        caps.workspace.fileOperations,
        { willRename: { filters: [{ pattern: { glob: '**/*' }, scheme: 'file' }] } },
        'fileOperations'
    );
    assert(caps.codeActionProvider.codeActionKinds.includes('refactor.move'), 'refactor.move kind');

    const beam = uriOf('custom', 'macros', 'secframing', 'beam.prg');
    const caller = uriOf('custom', 'macros', 'caller.prg');

    // 2. Code action: explicit refactor request or invoked, never automatic.
    const callerText = fs.readFileSync(path.join(CUSTOM, 'macros', 'caller.prg'), 'utf8');
    session.didOpen({ uri: caller, version: 7, text: `${callerText}z = call("$DIR_MACROS/secframing/beam.prg")\n` });
    const range = { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } };
    const refactor = await session.request('textDocument/codeAction', {
        textDocument: { uri: caller }, range, context: { diagnostics: [], only: ['refactor'] },
    });
    assertEqual(refactor.map((a) => [a.kind, a.command.command]), [['refactor.move', 'vertexadt.moveProgram']],
        'refactor action');
    const automatic = await session.request('textDocument/codeAction', {
        textDocument: { uri: caller }, range, context: { diagnostics: [], triggerKind: 2 },
    });
    assertEqual(automatic, [], 'no action on the automatic lightbulb');
    const invoked = await session.request('textDocument/codeAction', {
        textDocument: { uri: caller }, range, context: { diagnostics: [], triggerKind: 1 },
    });
    assertEqual(invoked.length, 1, 'action on Ctrl+.');

    // 3. Default target and validation.
    assertEqual(await move(session, { uri: beam, mode: 'default' }), { target: '$DIR_MACROS/secframing/beam.prg' },
        'default target');
    assertEqual(await move(session, { uri: beam, mode: 'validate', target: '$DIR_SYSTEM/macros/x.prg' }),
        { error: 'The target must be inside the Custom tree.' }, 'System target refused');
    assertEqual(await move(session, { uri: beam, mode: 'validate', target: '$DIR_COMPLIBS/tools/' }), { ok: true },
        'valid target');

    // 4. Plan: rename last, buffer wins with its version, report.
    const planned = await move(session, { uri: beam, mode: 'plan', target: '$DIR_COMPLIBS/tools/beam2.prg' });
    const changes = planned.edit.documentChanges;
    const rename = changes[changes.length - 1];
    assertEqual([rename.kind, sameUri(rename.oldUri, beam)], ['rename', true], 'rename op last');
    assert(rename.newUri.toLowerCase().endsWith('/custom/complibs/tools/beam2.prg'), `newUri ${rename.newUri}`);
    const callerChange = changes.find((item) => item.textDocument && sameUri(item.textDocument.uri, caller));
    assertEqual(callerChange.textDocument.version, 7, 'open buffer version');
    assertEqual(editsFor(changes, caller), Array(3).fill('$DIR_COMPLIBS/tools/beam2.prg'),
        'caller edits (buffer line included)');
    assertEqual(editsFor(changes, uriOf('custom', 'macros', 'secframing', 'frag.p')),
        ['$DIR_COMPLIBS/tools/beam2.prg'], '##included_from');
    assertEqual(editsFor(changes, beam), ['$DIR_MACROS/secframing/frag.p', 'beam2.prg'], 'moved file own paths');
    assertEqual(planned.edit.changeAnnotations['vertexadt.programMove'].needsConfirmation, true, 'preview');
    assertEqual(planned.report.updatedCount, 7, 'updated count');
    assertEqual(planned.report.skipped.map((site) => site.reason).sort(),
        ['System caller (System files are never edited)', 'dynamic path or text mention',
            'dynamic path or text mention'], 'skipped sites');

    // 5. Refusal and shadow confirmation.
    const exists = await move(session, { uri: beam, mode: 'plan', target: '$DIR_MACROS/caller.prg' });
    assert(exists.refusal && exists.refusal.includes('already exists'), `refusal ${JSON.stringify(exists)}`);
    const shared = uriOf('custom', 'macros', 'shared.prg');
    const ask = await move(session, { uri: shared, mode: 'plan', target: '$DIR_MACROS/moved.prg' });
    assert(ask.confirmation && ask.confirmation.includes('system/macros/shared.prg'), `confirm ${JSON.stringify(ask)}`);
    const confirmed = await move(session, { uri: shared, mode: 'plan', target: '$DIR_MACROS/moved.prg', confirmShadow: true });
    assert(confirmed.edit, 'confirmed plan has an edit');

    // 6. willRenameFiles: folder drag, text edits only, report notification.
    session._notifications.length = 0;
    const dragged = await session.request('workspace/willRenameFiles', {
        files: [{ oldUri: uriOf('custom', 'macros', 'secframing'), newUri: uriOf('custom', 'macros', 'sf') }],
    });
    assert(dragged && dragged.changes && !dragged.documentChanges, 'plain changes');
    const callerEdits = Object.entries(dragged.changes).find(([uri]) => sameUri(uri, caller));
    assertEqual(callerEdits[1].map((edit) => edit.newText), Array(3).fill('$DIR_MACROS/sf/beam.prg'),
        'callers follow the folder (case style not kept: new segment)');
    assert(!Object.keys(dragged.changes).some((uri) => uri.toLowerCase().includes('sibling')),
        'relative include inside the folder untouched');
    await session.waitQuiet(200);
    const report = session._notifications.find((message) => message.method === 'vertexadt/programMoveReport');
    assert(report && report.params.updatedCount > 0, 'report notification');

    const plain = await session.request('workspace/willRenameFiles', {
        files: [{ oldUri: uriOf('custom', 'notes.txt'), newUri: uriOf('custom', 'n.txt') }],
    });
    assertEqual(plain, null, 'non-ADT rename: null');

    await session.shutdownAndExit();
}

module.exports = { run };

if (require.main === module) {
    run().then(
        () => console.log('61-program-move passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
