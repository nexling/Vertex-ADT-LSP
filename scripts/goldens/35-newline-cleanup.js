'use strict';

/**
 * Parity goldens for newline cleanup (textDocument/onTypeFormatting).
 * Assertions from test-fixtures/newline-cleanup/TESTING.md (mirrors Node's
 * CleanUpLineFunc, reached today through the host watcher and
 * custom/cleanupLine). Buffers only: no fixture files.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'newline-cleanup');

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

// Input line → the line Node's host would leave after Enter (null = no edit:
// Node returned null, an identity rebuild, or Node threw).
const REWRITES = [
    ['set_obj_param(obj) NAME, 1', 'set_obj_param(obj) NAME, "1"'],
    ['set_obj_param(obj) NAME, "1"', null],
    ['set_obj_param(obj) NAME, VAL', null],
    ['set_obj_param(obj) NAME, "foo"', null],
    ['    set_obj_param(obj) NAME, 1', 'set_obj_param(obj) NAME, "1"'],
    ['\tset_obj_param(obj) NAME, "1"', 'set_obj_param(obj) NAME, "1"'],
    ['set_obj_param(obj) NAME, 1  ;! keep', 'set_obj_param(obj) NAME, "1";! keep'],
    ['set_obj_param(obj) NAME, "1" ;! keep', 'set_obj_param(obj) NAME, "1";! keep'],
    ['x = set_obj_param(obj) NAME, 1', 'set_obj_param(obj) NAME, "1"'],
    ['set_obj_param( obj )  NAME,  1', 'set_obj_param(obj) NAME, "1"'],
    ['set_obj_param(a,b ,  c) NAME, 1', 'set_obj_param(a, b, c) NAME, "1"'],
    ['set_obj_param(obj) NAME, 1.5', 'set_obj_param(obj) NAME, "1.5"'],
    ['set_obj_param(obj) NAME, 1.', null],
    ['set_obj_param(obj) NAME, .5', null],
    ['set_obj_param(obj) NAME, -1', null],
    ['set_obj_param(obj) NAME, 1e3', null],
    ['set_obj_param(obj) NAME, 1, 0, other', 'set_obj_param(obj) NAME, "1", 0, other'],
    ['set_obj_param(obj) NAME, 1, 0, other, extra', 'set_obj_param(obj) NAME, "1", 0, other, extra'],
    ['set_obj_param(obj) NAME, abc, 1', null],
    ['SET_OBJ_PARAM(obj) NAME, 1', 'SET_OBJ_PARAM(obj) NAME, "1"'],
    ['set_obj_param(obj) "NAME", 8', 'set_obj_param(obj) "NAME", "8"'],
    ['set_obj_param(obj) 5, 8', 'set_obj_param(obj) 5, "8"'],
    ['set_obj_param(obj) ";!x", 1', 'set_obj_param(obj) ";!x", "1"'],
    ['set_obj_param(obj) , 1', 'set_obj_param(obj) , "1"'],
    ['set_obj_param(obj) NAME, "a" & b$', null],
    ['set_djparam(jnt) P, 2', 'set_djparam(jnt) P, "2"'],
    ['set_joint_param(j) P, 3', 'set_joint_param(j) P, "3"'],
    ['set_joint_param(j) P, 3, 4', 'set_joint_param(j) P, "3", 4'],
    ['set_obj_param(obj) NAME, 1 ; plain comment', null],
    // Node null: command has no cleanupRules.
    ['get_obj_param(obj) NAME, 1', null],
    ['set_joint_punch(j) P, 1', null],
    // Node throws: parse miss, unknown command, missing quoted slot.
    ['', null],
    ['if a then', null],
    ['set_obj_param(obj NAME, 1', null],
    ['frobnicate(obj) NAME, 1', null],
    ['set_obj_param(obj) NAME', null],
    ['set_obj_param(obj)', null],
    ['set_djparam(jnt) P', null],
    ['! set_obj_param(obj) NAME, 1', null],
];

class Probe {
    constructor(session, fileName, languageId = 'vertexadt') {
        this.session = session;
        this.uri = pathToFileURL(path.join(FIXTURE, fileName)).href;
        this.languageId = languageId;
        this.version = 0;
    }

    sync(text) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({
                uri: this.uri,
                languageId: this.languageId,
                version: 1,
                text,
            });
        } else {
            this.session.didChange({
                uri: this.uri,
                version: this.version,
                contentChanges: [{ text }],
            });
        }
    }

    async format(text, line, character, ch = '\n') {
        this.sync(text);
        return this.session.request('textDocument/onTypeFormatting', {
            textDocument: { uri: this.uri },
            position: { line, character },
            ch,
            options: { tabSize: 4, insertSpaces: true },
        });
    }

    // Buffer after Enter at the end of `line`: `line\n<indent>`; cursor on the new line.
    async enterAfter(line, { before = [], indent = '', eol = '\n' } = {}) {
        const text = [...before, line, indent].join(eol);
        return this.format(text, before.length + 1, indent.length);
    }
}

function utf16Length(text) {
    return text.length;
}

function expectRewrite(result, lineIndex, input, expected, name) {
    if (expected === null) {
        assertEqual(result, null, `${name}: no edit`);
        return;
    }
    assertEqual(
        result,
        [
            {
                range: {
                    start: { line: lineIndex, character: 0 },
                    end: { line: lineIndex, character: utf16Length(input) },
                },
                newText: expected,
            },
        ],
        name
    );
}

async function run(session) {
    const init = await session.initialize();
    assertEqual(
        init.capabilities.documentOnTypeFormattingProvider,
        { firstTriggerCharacter: '\n' },
        'documentOnTypeFormattingProvider'
    );

    const wall = new Probe(session, 'wall.prg');
    for (const [input, expected] of REWRITES) {
        const result = await wall.enterAfter(input);
        expectRewrite(result, 0, input, expected, JSON.stringify(input));
    }

    // Previous line only, whatever sits above it; auto-indent on the new line.
    expectRewrite(
        await wall.enterAfter('set_djparam(j) P, 2', {
            before: ['set_obj_param(obj) NAME, 1', '#Macros#'],
            indent: '    ',
        }),
        2,
        'set_djparam(j) P, 2',
        'set_djparam(j) P, "2"',
        'previous line only, new line indented'
    );

    // Enter mid-line: the left fragment is the cleaned line.
    expectRewrite(
        await wall.format('set_obj_param(obj) NAME, 1\n ;! tail', 1, 0),
        0,
        'set_obj_param(obj) NAME, 1',
        'set_obj_param(obj) NAME, "1"',
        'mid-line Enter cleans the left fragment'
    );

    // CRLF buffer: the range ends before \r.
    expectRewrite(
        await wall.enterAfter('set_obj_param(obj) NAME, 1', { eol: '\r\n' }),
        0,
        'set_obj_param(obj) NAME, 1',
        'set_obj_param(obj) NAME, "1"',
        'CRLF line ending excluded'
    );

    // Range end is UTF-16.
    const astral = 'set_obj_param(obj) NAME, 1 ;! 𝒳 é';
    expectRewrite(
        await wall.enterAfter(astral),
        0,
        astral,
        'set_obj_param(obj) NAME, "1";! 𝒳 é',
        'UTF-16 range end'
    );

    // Not a newline trigger, or no previous line.
    assertEqual(await wall.format('set_obj_param(obj) NAME, 1', 0, 26, ';'), null, 'ch other than \\n');
    assertEqual(await wall.format('set_obj_param(obj) NAME, 1', 0, 26), null, 'no previous line');

    // Non-vertexadt never rewrites.
    for (const languageId of ['plaintext', 'log']) {
        const other = new Probe(session, `other.${languageId}`, languageId);
        assertEqual(
            await other.enterAfter('set_obj_param(obj) NAME, 1'),
            null,
            `${languageId} is null`
        );
    }

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'newline-cleanup' }],
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
            console.log('lsp golden: 35-newline-cleanup passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main, REWRITES };
