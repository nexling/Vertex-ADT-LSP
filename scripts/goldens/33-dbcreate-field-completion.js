'use strict';

/**
 * Parity goldens for dbcreate field completion.
 * Assertions from test-fixtures/dbcreate-field-completion/TESTING.md
 * (mirrors the Node host helpers in getDbcreateCompletionContext /
 * buildDbcreateCompletionItems). Local-only: no listing fixtures.
 */

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'dbcreate-field-completion');
const TYPICAL = 'dbcreate(1) "p",1,0,';
const INVOKED = 1;
const TYPED = 2;

const TYPE_LABELS = ['A', 'N', 'B', 'T', 'M', 'U'];
const LENGTH_LABELS = ['1', '8', '16', '32', '64', '128', '255'];
const INDEX_LABELS = ['x', 'IND', 'UNIQUE'];

const FAMILY_DETAILS = new Set([
    'wanted fielname - case sensitive, usually uppercase!',
    'Alpha field type',
    'Numeric field type',
    'Binary field type',
    'Text field type',
    'Memo field type',
    'Unicode field type',
    'Field length',
    'No index',
    'Indexed field',
    'Unique indexed field',
]);

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

function labels(items) {
    return items.map((item) => item.label);
}

function familyItems(result) {
    assert(result && Array.isArray(result.items), 'vertexadt completion is a CompletionList');
    // isIncomplete: checked by completion-ranking familyItems (true with family items).
    assert(
        result.items.some((item) => item.label === 'askdim'),
        'keeps the cheap keyword-variable list'
    );
    return ranking.familyItems(result, 'dbcreateField', 'dbcreate completion')
        .filter((item) => FAMILY_DETAILS.has(item.detail));
}

function expectNone(items, name) {
    assertEqual(labels(items), [], `${name} adds no dbcreate field items`);
}

function expectBareItem(item, name, typed) {
    assert(item.textEdit === undefined, `${name} has no textEdit`);
    assertEqual(item.filterText, typed, `${name} filterText is the typed word`);
    assertEqual(item.sortText, item.label, `${name} family key is the label`);
    assert(item.data === undefined, `${name} has no data`);
    assert(item.documentation === undefined, `${name} has no documentation`);
    assertEqual(item.insertText, item.label, `${name} insertText is the label`);
}

class Probe {
    constructor(session, uri) {
        this.session = session;
        this.uri = uri;
        this.version = 0;
    }

    async complete(text, line, character, languageId, context) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({
                uri: this.uri,
                languageId: languageId || 'vertexadt',
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
        const params = {
            textDocument: { uri: this.uri },
            position: { line, character },
        };
        if (context) {
            params.context = context;
        }
        return this.session.request('textDocument/completion', params);
    }

    async family(text, line, character, context) {
        return familyItems(await this.complete(text, line, character, 'vertexadt', context));
    }

    async atEnd(text, context) {
        const lines = text.split('\n');
        const line = lines.length - 1;
        return this.family(text, line, lines[line].length, context);
    }
}

// Typing a word filters the slot that starts at the word (substring, case-insensitive).
function expectFiltered(items, name, expectedLabels, typed) {
    assertEqual(labels(items), expectedLabels, `${name} labels`);
    for (const item of items) {
        expectBareItem(item, `${name} ${item.label}`, typed);
    }
}

function expectSlot0(items, name) {
    assertEqual(labels(items), ['FIELDNAME'], `${name} labels`);
    assertEqual(items[0].kind, 20, `${name} kind EnumMember`);
    assertEqual(
        items[0].detail,
        'wanted fielname - case sensitive, usually uppercase!',
        `${name} FIELDNAME typo detail`
    );
    expectBareItem(items[0], name);
}

function expectTypes(items, name) {
    assertEqual(labels(items), TYPE_LABELS, `${name} labels`);
    const details = [
        'Alpha field type',
        'Numeric field type',
        'Binary field type',
        'Text field type',
        'Memo field type',
        'Unicode field type',
    ];
    items.forEach((item, index) => {
        assertEqual(item.kind, 20, `${name} ${item.label} kind EnumMember`);
        assertEqual(item.detail, details[index], `${name} ${item.label} detail`);
        expectBareItem(item, `${name} ${item.label}`);
    });
}

function expectLengths(items, name) {
    assertEqual(labels(items), LENGTH_LABELS, `${name} labels`);
    for (const item of items) {
        assertEqual(item.kind, 12, `${name} ${item.label} kind Value`);
        assertEqual(item.detail, 'Field length', `${name} ${item.label} detail`);
        expectBareItem(item, `${name} ${item.label}`);
    }
}

function expectIndex(items, name) {
    assertEqual(labels(items), INDEX_LABELS, `${name} labels`);
    const details = ['No index', 'Indexed field', 'Unique indexed field'];
    items.forEach((item, index) => {
        assertEqual(item.kind, 20, `${name} ${item.label} kind EnumMember`);
        assertEqual(item.detail, details[index], `${name} ${item.label} detail`);
        expectBareItem(item, `${name} ${item.label}`);
    });
}

async function run(session) {
    const init = await session.initialize();
    assertEqual(
        init.capabilities.completionProvider.triggerCharacters,
        [' ', ',', '(', '/', '"', ')'],
        'completion trigger characters stay ticket 09 union'
    );
    assertEqual(init.capabilities.completionProvider.resolveProvider, true, 'resolve still advertised');

    const probe = new Probe(
        session,
        pathToFileURL(path.join(FIXTURE, 'dbcreate_probe.prg')).href
    );

    expectSlot0(await probe.atEnd(TYPICAL), 'after 3rd comma (no space)');
    expectSlot0(await probe.atEnd(TYPICAL + ' '), 'after 3rd comma + space');
    expectSlot0(await probe.atEnd(TYPICAL + '   '), 'only whitespace field text');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME'), 'typing first field NAME', ['FIELDNAME'], 'NAME');
    expectTypes(await probe.atEnd(TYPICAL + ' NAME '), 'after NAME + space');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME A'), 'typing type A', ['A'], 'A');
    expectLengths(await probe.atEnd(TYPICAL + ' NAME A '), 'after type + space');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME A 64'), 'typing length 64', ['64'], '64');
    expectIndex(await probe.atEnd(TYPICAL + ' NAME A 64 '), 'after length + space');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME A 64 x'), 'typing index x', ['x'], 'x');
    expectSlot0(await probe.atEnd(TYPICAL + ' NAME A 64 x '), 'after index + space (next field)');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME A 64 x NEXT'), 'second field name', [], 'NEXT');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME A 64 x NEXT A 8 IND'), 'complete two fields', ['IND'], 'IND');

    expectNone(await probe.atEnd('dbcreate(1'), 'inside parens');
    expectNone(await probe.atEnd('dbcreate(1)'), 'after close paren, 0 commas');
    expectNone(await probe.atEnd('dbcreate(1) "p",'), 'one comma');
    expectNone(await probe.atEnd('dbcreate(1) Dbase$,0,DBCopyId'), 'two commas (mode 0 copy)');
    expectNone(await probe.atEnd('dbcreate(1) Dbase$,0,DBCopyId '), 'two commas + space');
    expectNone(await probe.atEnd('dbcreate(1) "p,1,0,'), 'unclosed quote path then commas');
    expectNone(await probe.atEnd('xdbcreate(1) "p",1,0, '), 'xdbcreate not a site');
    expectNone(await probe.atEnd('_dbcreate(1) "p",1,0, '), '_dbcreate not a site');
    expectNone(await probe.atEnd('dbcreate 1, "p",1,0, '), 'no paren not a site');
    expectNone(
        await probe.atEnd('dbcreate(foo(1) "p",1'),
        'first ) too early not enough commas'
    );

    expectSlot0(await probe.atEnd('DBCREATE(1) "p",1,0, '), 'DBCREATE case');
    expectSlot0(await probe.atEnd('dbcreate (1) "p",1,0, '), 'space before paren');
    expectSlot0(await probe.atEnd('x = 1; dbcreate(1) "p",1,0, '), 'mid-line after other text');
    expectSlot0(await probe.atEnd('x = dbcreate(1) "p",1,0, '), 'assignment form still a site');
    expectSlot0(await probe.atEnd('    dbcreate(1) "p",1,0, '), 'indent');
    expectSlot0(await probe.atEnd('dbcreate() "p",1,0, '), 'empty parens still a site');
    expectSlot0(await probe.atEnd('dbcreate(foo(1)) "p",1,0, '), 'nested paren handle');
    expectSlot0(await probe.atEnd('dbcreate(foo(1,2)) "p",1,0, '), 'nested handle inner comma');
    expectSlot0(await probe.atEnd('dbcreate(1) "a,b",1,0, '), 'quoted path with comma');
    expectSlot0(await probe.atEnd("dbcreate(1) 'a,b',1,0, "), 'single-quoted path with comma');

    expectFiltered(
        await probe.atEnd('dbcreate(1) path,with,comma,1,0, FIELD'),
        'unquoted path commas steal slots',
        [],
        'FIELD'
    );
    expectFiltered(await probe.atEnd(TYPICAL + 'FIELD'), '3rd comma glued field', ['FIELDNAME'], 'FIELD');
    expectFiltered(await probe.atEnd(TYPICAL + ' FIELDNAME'), 'FIELDNAME placeholder as token', ['FIELDNAME'], 'FIELDNAME');
    expectFiltered(await probe.atEnd(TYPICAL + ' A\\B'), 'mid-line backslash splits tokens', ['B'], 'B');
    expectTypes(await probe.atEnd(TYPICAL + ' NAME,'), 'comma glued to field token (one token)');
    expectTypes(await probe.atEnd(TYPICAL + '\tNAME\t'), 'tabs as whitespace');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME ;! comment'), ';! after fields on same line still a site', [], 'comment');
    expectNone(await probe.atEnd('dbcreate(1) "p;!x",1,0, NAME'), 'quoted ;! in path strips');
    expectFiltered(await probe.atEnd(TYPICAL + ' NAME ! not stripped'), 'bang-only comment does not strip', [], 'stripped');

    const continued = 'dbcreate(1) "p",1,0, \\\nNAME A 64 x';
    expectSlot0(
        await probe.family(continued, 0, 'dbcreate(1) "p",1,0, \\'.length),
        'continuation start line after 3rd comma + \\'
    );
    expectSlot0(await probe.family(continued, 1, 0), 'continuation line at col 0');
    expectFiltered(await probe.family(continued, 1, 4), 'continuation line after NAME', ['FIELDNAME'], 'NAME');
    expectFiltered(await probe.atEnd(continued), 'continuation line at end (complete group)', ['x'], 'x');

    const continuedEmpty = 'dbcreate(1) "p",1,0, \\\nfoo';
    expectSlot0(await probe.family(continuedEmpty, 1, 0), 'new empty continuation line col 0');
    expectSlot0(await probe.family('dbcreate(1) "p",1,0, \\\n   ', 1, 3), 'new continuation line after spaces');
    expectFiltered(await probe.family('dbcreate(1) "p",1,0, \\\nF', 1, 1), 'new continuation line typing F', ['FIELDNAME'], 'F');

    const continuedFull = 'dbcreate(1) "p",1,0, \\\nNAME A 64 x \\\nNEXT';
    expectSlot0(
        await probe.family(continuedFull, 2, 0),
        'after complete field group, next line col 0'
    );
    expectFiltered(
        await probe.family('dbcreate(1) "p",1,0, \\\nNAME A 64 x \\\nN', 2, 1),
        'after complete field group, typing N',
        ['FIELDNAME'],
        'N'
    );
    expectFiltered(
        await probe.family('dbcreate(1) "p",1,0, \\\nNAME A 64 x \\\n N', 2, 2),
        'after complete field group, space then N',
        ['FIELDNAME'],
        'N'
    );

    expectNone(
        await probe.atEnd('foo \\\nbar \\\ndbcreate(1) "p",1,0, '),
        'unrelated previous continuation hides dbcreate'
    );
    expectNone(
        await probe.family('something \\\ndbcreate(1) "p",1,0, \\\nFIELD A 64 x', 2, 5),
        'dbcreate in middle of other continuation chain'
    );
    expectNone(
        await probe.family('dbcreate(1) "p",1,0, NAME A 64 x ;! comment \\\nNEXT', 1, 4),
        ';! before trailing \\ breaks walk'
    );

    const midToken = TYPICAL + ' FIELDNAME';
    expectFiltered(
        await probe.family(midToken, 0, (TYPICAL + ' FIEL').length),
        'cursor mid-token FIELD',
        ['FIELDNAME'],
        'FIEL'
    );
    expectNone(
        await probe.family('dbcreate(1) "p",1,0, NAME', 0, 'dbcreate(1) "p",1'.length),
        'cursor before 3rd comma (in mode)'
    );
    expectNone(
        await probe.family('dbcreate(handle) "p",1,0, NAME', 0, 'dbcreate(han'.length),
        'cursor inside handle'
    );

    const typed = await probe.atEnd(TYPICAL + ' ', { triggerKind: TYPED, triggerCharacter: ' ' });
    const invoked = await probe.atEnd(TYPICAL + ' ', { triggerKind: INVOKED });
    const noContext = await probe.atEnd(TYPICAL + ' ');
    assertEqual(labels(typed), ['FIELDNAME'], 'typed space still slot 0');
    assertEqual(labels(invoked), labels(typed), 'Invoked ignored');
    assertEqual(labels(noContext), labels(typed), 'missing context ignored');

    const withKeyword = await probe.complete(TYPICAL + ' ', 0, (TYPICAL + ' ').length);
    assert(
        withKeyword.items.some((item) => item.label === 'dbcreate' && item.data !== undefined),
        'cheap dbcreate keyword stays visible beside FIELDNAME'
    );

    const fieldname = familyItems(withKeyword)[0];
    const resolved = await session.completionResolve(fieldname);
    assertEqual(resolved.documentation, undefined, 'resolve does not fill family items');
    assertEqual(resolved.data, undefined, 'resolve leaves family data unset');

    const plainUri = pathToFileURL(path.join(FIXTURE, 'plain.txt')).href;
    probe.session.didOpen({
        uri: plainUri,
        languageId: 'plaintext',
        version: 1,
        text: TYPICAL + ' ',
    });
    const plain = await probe.session.request('textDocument/completion', {
        textDocument: { uri: plainUri },
        position: { line: 0, character: (TYPICAL + ' ').length },
    });
    assertEqual(plain, null, 'non-vertexadt is null');

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'dbcreate-field-completion' }],
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
            console.log('lsp golden: 33-dbcreate-field-completion passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
