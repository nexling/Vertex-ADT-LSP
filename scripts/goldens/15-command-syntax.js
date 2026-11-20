'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness.js');

const ROOT = path.resolve(__dirname, '..', '..');
const SOURCE = 'Syntax exception - ADT Plugin';
const ERROR = 1;
const WARNING = 2;

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

function syntaxItems(result) {
    return (result && result.items ? result.items : [])
        .filter((diagnostic) => diagnostic.source === SOURCE)
        .map((diagnostic) => ({
            message: diagnostic.message,
            source: diagnostic.source,
            severity: diagnostic.severity,
            line: diagnostic.range && diagnostic.range.start ? diagnostic.range.start.line : undefined,
            startCharacter: diagnostic.range && diagnostic.range.start ? diagnostic.range.start.character : undefined,
            endCharacter: diagnostic.range && diagnostic.range.end ? diagnostic.range.end.character : undefined,
        }));
}

async function pull(session, name, text, version = 1) {
    const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', name)).href;
    session.didOpen({ uri, languageId: 'vertexadt', version, text });
    const result = await session.diagnostic(uri);
    assertEqual(result && result.kind, 'full', `${name} pull kind`);
    return { uri, result, items: syntaxItems(result), lines: text.split('\n') };
}

function expectWholeLine(item, lineText, label) {
    assertEqual(item.startCharacter, 0, `${label} range start`);
    assertEqual(item.endCharacter, lineText.length, `${label} range end`);
    assertEqual(item.source, SOURCE, `${label} source`);
}

async function run(session) {
    await session.initialize();

    const emptyComma = await pull(
        session,
        '.golden-15-empty-comma.prg',
        [
            'vec_times_scalar(joinstartP),shtlaytopN,jointholestart',
            'vec_times_scalar(NewVector) Vector, ScaleFactor',
            'vec_times_scalar() a, b',
            'vec_times_scalar(out) a, b,',
            'vec_times_scalar(,x) a, b',
            'dbsave(database),',
            '',
        ].join('\n')
    );
    const emptyMessages = emptyComma.items.map((item) => item.message);
    assertEqual(
        emptyComma.items.filter((item) => item.line === 0).map((item) => item.message),
        [
            'First parameter after parentheses is missing in vec_times_scalar',
            'vec_times_scalar accepts at most 2 parameters after parentheses',
        ],
        'empty first after-paren slot emits missing and at-most-2'
    );
    assertEqual(
        emptyComma.items.filter((item) => item.line === 1),
        [],
        'valid vec_times_scalar has no diagnostics'
    );
    assertEqual(
        emptyComma.items.filter((item) => item.line === 2).map((item) => item.message),
        ['vec_times_scalar requires at least 1 parameters inside parentheses'],
        '() is zero inside params and uses 1 parameters wording'
    );
    assertEqual(
        emptyComma.items.filter((item) => item.line === 3).map((item) => item.message),
        ['vec_times_scalar accepts at most 2 parameters after parentheses'],
        'empty slot past arity is too-many, not missing'
    );
    assertEqual(
        emptyComma.items.filter((item) => item.line === 4).map((item) => item.message),
        [
            'First parameter inside parentheses is missing in vec_times_scalar',
            'vec_times_scalar accepts at most 1 parameters inside parentheses',
        ],
        'empty inside slot reports missing first then too-many inside'
    );
    assertEqual(
        emptyComma.items.filter((item) => item.line === 5).map((item) => item.message),
        ['dbsave accepts at most 0 parameters after parentheses'],
        'maxParamsOutside 0 does not report missing after-paren slots'
    );
    for (const item of emptyComma.items) {
        expectWholeLine(item, emptyComma.lines[item.line], `empty-comma line ${item.line}`);
        assertEqual(item.severity, ERROR, `empty-comma line ${item.line} severity`);
    }
    assert(emptyMessages.length > 0, 'empty-comma fixture produced diagnostics');

    const asciiValid = await pull(
        session,
        '.golden-15-ascii-valid.prg',
        'said$ = replace(said$,ascii(35)&"SAID"&ascii(35),Panel_Label$)\n'
    );
    assertEqual(asciiValid.items, [], 'ascii(35) concat is a valid string expression on replace');

    const asciiInvalid = await pull(
        session,
        '.golden-15-ascii-invalid.prg',
        'said$ = replace(said$,ASCII(35),Panel_Label$)\n'
    );
    assertEqual(
        asciiInvalid.items.map((item) => item.message),
        ['Second parameter inside parentheses must be a valid string in replace'],
        'ASCII() is case-sensitive and is not a string expression'
    );
    assertEqual(asciiInvalid.items[0] && asciiInvalid.items[0].severity, ERROR, 'ascii invalid severity');
    expectWholeLine(asciiInvalid.items[0], asciiInvalid.lines[0], 'ascii invalid');

    const asciiSpace = await pull(
        session,
        '.golden-15-ascii-space.prg',
        'said$ = replace(said$,ascii (35),Panel_Label$)\n'
    );
    assertEqual(
        asciiSpace.items.map((item) => item.message),
        ['Second parameter inside parentheses must be a valid string in replace'],
        'space before ( is not ascii('
    );

    const bangDir = path.join(ROOT, 'test-fixtures', 'semicolon-bang-comments');
    const bangText = fs.readFileSync(path.join(bangDir, 'findstri_in_string.prg'), 'utf8');
    const bang = await pull(session, path.join('semicolon-bang-comments', 'findstri_in_string.prg'), bangText);
    const bangByLine = new Map(bang.items.map((item) => [item.line, item]));
    assert(!bangByLine.has(0), 'quoted ;! does not mismatch findstri parentheses');
    assert(!bangByLine.has(1), 'trailing ;! comment is stripped before findstri validation');
    assert(bangByLine.has(2), 'real ;! after broken syntax still reports a parenthesis error');
    assertEqual(
        bangByLine.get(2).message,
        "Mismatching parentheses for 'findstri'",
        'unclosed findstri after real ;! strip'
    );
    assertEqual(bangByLine.get(2).source, SOURCE, 'semicolon-bang source');
    assertEqual(bangByLine.get(2).severity, ERROR, 'semicolon-bang severity');
    // Native bracket balance (docs/adr/0006): from the unclosed `(` to the
    // last code character before the real `;!`, not the whole line.
    assertEqual(
        [bangByLine.get(2).startCharacter, bangByLine.get(2).endCharacter],
        [14, 25],
        'semicolon-bang broken line range'
    );

    const plane = await pull(
        session,
        '.golden-15-pnt-to-plane.prg',
        [
            'pnt_to_plane(D2, D, D, X, Y)',
            'pnt_to_plane(D2, D, D2, X, Y)',
            'pnt_to_plane(D2, D2, D3, X, Y)',
            '',
        ].join('\n')
    );
    assertEqual(
        plane.items.filter((item) => item.line === 0).map((item) => ({
            message: item.message,
            severity: item.severity,
            source: item.source,
        })),
        [{
            message: 'pnt_to_plane: second parameter must not be the same as the third (plane point) parameter',
            severity: WARNING,
            source: SOURCE,
        }],
        'pnt_to_plane warns on 2nd vs 3rd only'
    );
    assertEqual(plane.items.filter((item) => item.line === 1), [], '1st===3rd is not the pnt_to_plane warning');
    assertEqual(plane.items.filter((item) => item.line === 2), [], 'in-place 2nd≠3rd does not warn');
    expectWholeLine(plane.items[0], plane.lines[0], 'pnt_to_plane warning');

    const firstOnly = await pull(
        session,
        '.golden-15-first-command.prg',
        'abs(x) acos(y)\n'
    );
    assertEqual(
        firstOnly.items,
        [],
        'first catalog command only: valid abs does not validate later acos'
    );

    const firstError = await pull(
        session,
        '.golden-15-first-error.prg',
        'vec_times_scalar() a, b abs(x)\n'
    );
    assertEqual(
        firstError.items.map((item) => item.message),
        ['vec_times_scalar requires at least 1 parameters inside parentheses'],
        'first catalog command error short-circuits later names'
    );

    const callBad = await pull(session, '.golden-15-call-var.prg', 'call(foo)\n');
    assertEqual(
        callBad.items.map((item) => item.message),
        ['First parameter inside parentheses must be a valid string or string variable in call'],
        'call path rejects a normal variable'
    );
    const callEmpty = await pull(session, '.golden-15-call-empty.prg', 'call()\n');
    assertEqual(
        callEmpty.items.map((item) => item.message),
        ['call requires at least 1 parameter inside parentheses'],
        'call() is zero inside params'
    );
    const callAscii = await pull(session, '.golden-15-call-ascii.prg', 'call(ascii(35))\n');
    assertEqual(
        callAscii.items.map((item) => item.message),
        ['First parameter inside parentheses must be a valid string or string variable in call'],
        'call path is not a string-expression and rejects ascii('
    );
    const callOk = await pull(session, '.golden-15-call-ok.prg', 'call("path")\n');
    assertEqual(callOk.items, [], 'quoted call path is valid');

    const strictOff = await pull(
        session,
        '.golden-15-strict-off.prg',
        'P = 1 ;!"[pvector] x"\nabs(P)\n'
    );
    assertEqual(strictOff.items, [], 'strict slots extra-gated off by default');

    session.didChangeConfiguration({
        vertexadt: {
            strictMode: { enabled: true },
        },
    });
    const strictOn = await pull(
        session,
        '.golden-15-strict-on.prg',
        'P = 1 ;!"[pvector] x"\nabs(P)\n'
    );
    assertEqual(
        strictOn.items.map((item) => item.message),
        ["abs: input value 'P' should be of type [value] but is 'pvector'"],
        'strict mode type slot mismatch uses the exact template'
    );
    assertEqual(strictOn.items[0] && strictOn.items[0].severity, ERROR, 'strict mismatch is Error');
    assertEqual(strictOn.items[0] && strictOn.items[0].source, SOURCE, 'strict mismatch source');
    expectWholeLine(strictOn.items[0], strictOn.lines[1], 'strict mismatch');
    assertEqual(strictOn.items[0].line, 1, 'strict mismatch paints the abs line');

    await session.shutdownAndExit();
}

async function main() {
    const session = LspSession.launch();
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
            console.log('15-command-syntax goldens passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main };
