'use strict';

/**
 * Parity goldens for include/call path and section completion.
 * Assertions from test-fixtures/include-call-sections/TESTING.md (mirrors the
 * former Node completion pins plus the listing fixtures under custom/completion
 * and system/completion).
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const { familyItems } = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'include-call-sections');
const CUSTOM = path.join(FIXTURE, 'custom');
const CALLEE = 'unknown_section_callee.prg';
const INVOKED = 1;
const TYPED = 2;

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

const DOLLAR_LABELS = JSON.parse(
    fs.readFileSync(path.join(ROOT, 'server', 'catalog', 'keywords.json'), 'utf8')
)
    .map((entry) => entry.label)
    .filter((label) => label.startsWith('$'));

class Probe {
    constructor(session, uri, languageId = 'vertexadt') {
        this.session = session;
        this.uri = uri;
        this.version = 1;
        session.didOpen({ uri, languageId, version: 1, text: '' });
    }

    async complete(lineText, character, triggerKind, triggerCharacter) {
        this.version += 1;
        this.session.didChange({
            uri: this.uri,
            version: this.version,
            contentChanges: [{ text: `x = 1\n${lineText}\ny = 2\n` }],
        });
        const params = {
            textDocument: { uri: this.uri },
            position: { line: 1, character: character === undefined ? lineText.length : character },
        };
        if (triggerKind !== undefined) {
            params.context = { triggerKind };
            if (triggerCharacter) {
                params.context.triggerCharacter = triggerCharacter;
            }
        }
        return this.session.request('textDocument/completion', params);
    }

    // The family's items (ranking contract checked, sortText back to the family key).
    async family(lineText, character, triggerKind, triggerCharacter) {
        const result = await this.complete(lineText, character, triggerKind, triggerCharacter);
        assert(result && Array.isArray(result.items), `${JSON.stringify(lineText)} returns a CompletionList`);
        // isIncomplete: checked by completion-ranking familyItems (true with family items).
        assert(
            result.items.some((item) => item.label === 'askdim'),
            `${JSON.stringify(lineText)} keeps the cheap keyword-variable list`
        );
        return familyItems(result, 'includeCall', JSON.stringify(lineText));
    }

    // A typed trigger character with no family items opens no popup
    // (docs/adr/0007-native-trigger-characters-open-family-sites-only.md).
    async noPopup(lineText, character, triggerCharacter) {
        const result = await this.complete(lineText, character, TYPED, triggerCharacter);
        assertEqual(result, { isIncomplete: false, items: [] }, `${JSON.stringify(lineText)} opens no popup`);
    }
}

function labels(items) {
    return items.map((item) => item.label);
}

// `typed`: the text being typed in the slot; sections are substring-filtered
// on it and take it as filterText (slot substring match).
function expectSections(items, lineText, names, { leadingSpace, replace, typed }) {
    assertEqual(labels(items), names, `${JSON.stringify(lineText)} section labels`);
    for (const item of items) {
        const insert = (leadingSpace ? ' ' : '') + item.label;
        assertEqual(item.kind, 20, `${item.label} kind EnumMember`);
        assertEqual(item.detail, 'section', `${item.label} detail`);
        assertEqual(item.sortText, item.label, `${item.label} sortText`);
        assertEqual(item.filterText, typed === undefined ? item.label : typed, `${item.label} filterText`);
        assertEqual(item.insertText, insert, `${item.label} insertText`);
        assert(item.textEdit, `${item.label} has textEdit`);
        assertEqual(item.textEdit.newText, insert, `${item.label} textEdit newText`);
        assertEqual(
            item.textEdit.range,
            {
                start: { line: 1, character: replace[0] },
                end: { line: 1, character: replace[1] },
            },
            `${JSON.stringify(lineText)} replace range`
        );
        assert(item.data === undefined, `${item.label} has no data`);
    }
}

function expectNone(items, lineText) {
    assertEqual(labels(items), [], `${JSON.stringify(lineText)} adds no include/call items`);
}

async function run(session) {
    const init = await session.initialize();
    assertEqual(
        init.capabilities.completionProvider.triggerCharacters,
        [' ', ',', '(', '/', '"', ')'],
        'completion trigger characters'
    );
    assertEqual(init.capabilities.completionProvider.resolveProvider, true, 'resolve still advertised');

    const probe = new Probe(session, pathToFileURL(path.join(CUSTOM, 'completion_probe.prg')).href);
    const names = ['JOINT', 'MACROS'];

    // Space after a file opens sections; no leading space.
    let line = `# include ${CALLEE} `;
    expectSections(await probe.family(line, undefined, TYPED, ' '), line, names, {
        leadingSpace: false,
        replace: [line.length, line.length],
    });

    // Ctrl+Space at end of path: sections with a leading space.
    line = `# include ${CALLEE}`;
    expectSections(await probe.family(line, undefined, INVOKED), line, names, {
        leadingSpace: true,
        replace: [line.length, line.length],
    });

    // Typing the filename stays paths (listing of a file → nothing).
    expectNone(await probe.family(line, undefined, TYPED), `${line} (typing)`);
    // No context and TriggerForIncompleteCompletions are not Invoked.
    expectNone(await probe.family(line, undefined, undefined), `${line} (no context)`);
    expectNone(await probe.family(line, undefined, 3), `${line} (incomplete)`);

    // Partial / complete / mid-token suffix replace the whole ident.
    // Typed text filters the sections (substring).
    line = `# include ${CALLEE} MA`;
    expectSections(await probe.family(line, undefined, TYPED), line, ['MACROS'], {
        leadingSpace: false,
        replace: [line.length - 2, line.length],
        typed: 'MA',
    });
    line = `# include ${CALLEE} MACROS`;
    expectSections(await probe.family(line, line.length - 3, TYPED), line, ['MACROS'], {
        leadingSpace: false,
        replace: [line.length - 6, line.length],
        typed: 'MAC',
    });
    line = `# include ${CALLEE} OIN`;
    expectSections(await probe.family(line, undefined, TYPED), line, ['JOINT'], {
        leadingSpace: false,
        replace: [line.length - 3, line.length],
        typed: 'OIN',
    });
    expectNone(await probe.family(`${line} `, undefined, TYPED), `${line} (past suffix)`);

    // SECTION / SECTIONOFFILE occupy the slot; as typed text they match no
    // section (VS Code filtered them out client-side before, too).
    line = `# include ${CALLEE} SECTIONOFFILE`;
    expectNone(await probe.family(line, undefined, INVOKED), line);

    // Quoted include: section after the closing quote, not glued to it.
    line = `# include "${CALLEE}" `;
    expectSections(await probe.family(line, undefined, TYPED), line, names, {
        leadingSpace: false,
        replace: [line.length, line.length],
    });
    expectNone(await probe.family(`# include "${CALLEE}"`, undefined, INVOKED), 'glued after quote');

    // Parenthesized call: inside the first quoted argument only.
    line = `call("${CALLEE} `;
    expectSections(await probe.family(line, undefined, TYPED), line, names, {
        leadingSpace: false,
        replace: [line.length, line.length],
    });
    line = `call("${CALLEE}")`;
    expectNone(await probe.family(line, line.length - 1, INVOKED), 'cursor on ) after call quote');
    line = `Ret$=call("${CALLEE}`;
    expectSections(await probe.family(line, undefined, INVOKED), line, names, {
        leadingSpace: true,
        replace: [line.length, line.length],
    });
    await probe.noPopup(`Ret$=call("${CALLEE}",`, undefined, ',');
    await probe.noPopup(`Ret$=call("${CALLEE} MACROS",`, undefined, ',');
    line = `call(${CALLEE} `;
    expectSections(await probe.family(line, undefined, TYPED), line, names, {
        leadingSpace: false,
        replace: [line.length, line.length],
    });

    // Space-form call.
    line = `call ${CALLEE} `;
    expectSections(await probe.family(line, undefined, TYPED), line, names, {
        leadingSpace: false,
        replace: [line.length, line.length],
    });

    // Directory, file-and-directory, nameless file → no picker.
    expectNone(await probe.family('# include unknown_section_dir ', undefined, TYPED), 'directory');
    expectNone(await probe.family('# include completion/both_target ', undefined, TYPED), 'both');
    expectNone(await probe.family('# include unknown_section_plain.prg ', undefined, TYPED), 'no names');
    expectNone(await probe.family('# include unknown_section_empty.prg ', undefined, TYPED), 'empty file');
    expectNone(await probe.family('# include missing_no_file.prg SECTION', undefined, TYPED), 'missing');

    // Start-anchored, lowercase directives only.
    expectNone(await probe.family(`x = 1; # include ${CALLEE} `, undefined, TYPED), 'mid-line include');
    expectNone(await probe.family(`# INCLUDE ${CALLEE} `, undefined, TYPED), '# INCLUDE');
    expectNone(await probe.family(`CALL ${CALLEE} `, undefined, TYPED), 'CALL');
    expectNone(await probe.family(`Ret$ = call("${CALLEE} `, undefined, TYPED), 'spaced assignment');

    // Empty path: only `$` keyword-catalog folders.
    for (const empty of ['# include ', 'call("', '##included_from ']) {
        const items = await probe.family(empty, undefined, TYPED);
        assertEqual(labels(items), DOLLAR_LABELS, `${JSON.stringify(empty)} $ folders`);
        for (const item of items) {
            assertEqual(item.kind, 19, `${item.label} kind Folder`);
            assertEqual(item.sortText, `0${item.label}`, `${item.label} sortText`);
            assert(item.insertText === undefined, `${item.label} inserts the label`);
            assertEqual(
                item.textEdit,
                {
                    range: {
                        start: { line: 1, character: empty.length },
                        end: { line: 1, character: empty.length },
                    },
                    newText: item.label,
                },
                `${item.label} replaces the typed segment`
            );
        }
    }

    // Non-empty path lists that folder: Custom then System, Custom name wins.
    const listing = ['sub/', 'alpha.prg', 'both_target', 'both_target/', 'sys_only.prg'];
    let items = await probe.family('# include completion/', undefined, TYPED, '/');
    assertEqual(labels(items), listing, 'include completion/ listing');
    const byLabel = Object.fromEntries(items.map((item) => [item.label, item]));
    assertEqual(byLabel['sub/'].kind, 19, 'sub/ Folder');
    assertEqual(byLabel['sub/'].insertText, 'sub/', 'sub/ insertText keeps slash');
    assertEqual(byLabel['sub/'].sortText, '0sub/', 'sub/ sortText');
    assertEqual(byLabel['alpha.prg'].kind, 17, 'alpha.prg File');
    assertEqual(byLabel['alpha.prg'].sortText, '1alpha.prg', 'alpha.prg sortText');
    assert(byLabel['alpha.prg'].insertText === undefined, 'file inserts the label');
    assertEqual(byLabel['alpha.prg'].detail, '2 sections', 'alpha.prg detail (Custom text)');
    assertEqual(
        byLabel['alpha.prg'].documentation,
        { kind: 'markdown', value: '- `ONE`\n- `TWO`' },
        'alpha.prg documentation'
    );
    assertEqual(byLabel.both_target.detail, '1 section', 'both_target detail');
    assert(byLabel['sys_only.prg'].detail === undefined, 'nameless file has no detail');
    for (const item of items) {
        assertEqual(item.textEdit.newText, item.label, `${item.label} path item replaces the typed segment`);
    }

    // ##included_from: paths forever, no attached names.
    items = await probe.family('##included_from completion/', undefined, TYPED, '/');
    assertEqual(labels(items), listing, 'included_from listing');
    assert(items.every((item) => item.detail === undefined), 'included_from attaches no names');
    expectNone(await probe.family(`##included_from ${CALLEE} `, undefined, TYPED), 'included_from after file');

    // $DIR_* first segment converts for listing.
    items = await probe.family('# include $DIR_MACROS/', undefined, TYPED, '/');
    assertEqual(labels(items), ['unknown_section_foo.prg'], '$DIR_MACROS/ listing');
    assertEqual(items[0].detail, '1 section', '$DIR_MACROS file names attached');

    // Non-vertexadt: null.
    const plain = new Probe(session, pathToFileURL(path.join(CUSTOM, 'completion_probe.txt')).href, 'plaintext');
    const plainResult = await plain.complete(`# include ${CALLEE} `, undefined, TYPED, ' ');
    assertEqual(plainResult, null, 'plaintext completion is null');

    // Buffer-wins linked-file snapshot.
    session.didOpen({
        uri: pathToFileURL(path.join(CUSTOM, CALLEE)).href,
        languageId: 'vertexadt',
        version: 1,
        text: '#FROM_BUFFER#\nq = 1\n#FROM_BUFFER#\n',
    });
    await session.waitQuiet();
    line = `# include ${CALLEE} `;
    expectSections(await probe.family(line, undefined, TYPED, ' '), line, ['FROM_BUFFER'], {
        leadingSpace: false,
        replace: [line.length, line.length],
    });

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
            console.log('lsp golden: 32-include-call-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
