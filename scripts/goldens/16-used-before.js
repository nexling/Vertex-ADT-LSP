#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'validation-isolation');

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

function innerOf(message) {
    const match = String(message).match(/has an error: (.*)$/);
    return match ? match[1] : String(message);
}

function usedBeforeItems(items) {
    return (items || []).filter((item) => {
        const inner = innerOf(item.message);
        return inner.includes('used before declaration')
            || inner.includes('exceeds maximum length of 16 characters');
    });
}

function inners(items) {
    return usedBeforeItems(items).map((item) => innerOf(item.message));
}

function assignmentItems(items) {
    return (items || []).filter((item) => innerOf(item.message).includes('used before assignment'));
}

function assignmentInners(items) {
    return assignmentItems(items).map((item) => innerOf(item.message));
}

function wrapped(lineText, inner) {
    return `${lineText} has an error: ${inner}`;
}

async function pullText(session, name, text) {
    const uri = pathToFileURL(path.join(FIXTURE, name)).href;
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    const pull = await session.diagnostic(uri);
    assertEqual(pull && pull.kind, 'full', `${name} pull kind`);
    return { uri, items: pull.items || [] };
}

// Used before declaration is an Error (1); the length message stays a Warning (2).
function assertEnvelope(item, lineText, inner, line, severity) {
    assertEqual(item.severity, severity, `${inner} severity`);
    assertEqual(item.source, 'ADT-Syntax', `${inner} source`);
    assertEqual(item.message, wrapped(lineText, inner), `${inner} wrapped message`);
    assert(item.code === undefined, `${inner} has no code`);
    assertEqual(item.range && item.range.start && item.range.start.line, line, `${inner} start line`);
    assertEqual(item.range.start.character, 1, `${inner} start character skips first char`);
    assertEqual(item.range.end.character, lineText.length, `${inner} end character`);
    const related = item.relatedInformation;
    assert(Array.isArray(related) && related.length === 1, `${inner} relatedInformation`);
    assertEqual(related[0].message, inner, `${inner} related inner`);
}

async function run() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'validation-isolation' }],
    });
    try {
        await session.initialize();

        const undeclared = await pullText(session, 'used-before-generic.prg', 'z = missing\n');
        assertEqual(inners(undeclared.items), ['Variable missing used before declaration'], 'generic undeclared');
        assertEnvelope(
            usedBeforeItems(undeclared.items)[0],
            'z = missing',
            'Variable missing used before declaration',
            0,
            1
        );

        const sourceOrder = await pullText(
            session,
            'used-before-source-order.prg',
            'z = later\nlater = 1\n'
        );
        assertEqual(inners(sourceOrder.items), [], 'later assignment still counts as declared');
        assertEqual(
            assignmentInners(sourceOrder.items),
            ['Variable later used before assignment (first assigned on line 2)'],
            'later assignment is used before assignment'
        );
        {
            const item = assignmentItems(sourceOrder.items)[0];
            assertEqual(item.severity, 2, 'used-before-assignment severity');
            assertEqual(item.source, 'ADT-Syntax', 'used-before-assignment source');
            assertEqual(
                item.message,
                wrapped('z = later', 'Variable later used before assignment (first assigned on line 2)'),
                'used-before-assignment wrapped message'
            );
            assertEqual(item.range.start.line, 0, 'used-before-assignment on the use line');
            const related = item.relatedInformation;
            assert(Array.isArray(related) && related.length === 1, 'used-before-assignment relatedInformation');
            assertEqual(related[0].message, 'later is first assigned here', 'related message');
            assertEqual(related[0].location.range.start.line, 1, 'related points at the assignment line');
            assertEqual(related[0].location.range.start.character, 0, 'related start character');
            assertEqual(related[0].location.range.end.character, 'later = 1'.length, 'related end character');
        }

        const hyphen = await pullText(session, 'used-before-hyphen.prg', 'my-var = 1\n');
        assertEqual(
            inners(hyphen.items),
            ['Variable my used before declaration', 'Variable var used before declaration'],
            'hyphen splits generic matches'
        );

        // A later `$` still hides numeric names (`foo`), but the `bar$` use itself is checked.
        const laterDollar = await pullText(session, 'used-before-later-dollar.prg', 'foo = bar$\n');
        assertEqual(
            inners(laterDollar.items),
            ['Variable bar$ used before declaration'],
            'later $ suppresses numeric identifiers only'
        );

        // --- String variables in the generic pass (ADR 0024) ---
        const studText = [
            'Code$= get(SC)',
            'studnum = get(SN)',
            'makes3 = get("W~MAKES3")',
            'if makes3 = 1 and findstri(code$,"x95") then',
            '  Detail$ = "WD-STUD"&studnum&"F"',
            'else',
            '  Detail$ = "WD-STUD"&studnum',
            'endif',
            'set WALL_DET_NAME Detail$',
            '',
        ].join('\n');
        const stud = await pullText(session, 'used-before-string-stud.prg', studText);
        assertEqual(
            inners(stud.items),
            ['Variable code$ used before declaration (did you mean Code$?)'],
            'wrong-case string variable is undeclared, with a hint'
        );
        assertEnvelope(
            usedBeforeItems(stud.items)[0],
            'if makes3 = 1 and findstri(code$,"x95") then',
            'Variable code$ used before declaration (did you mean Code$?)',
            3,
            1
        );

        const stringCases = [
            ['undeclared', 'z$ = s$\n', ['Variable s$ used before declaration']],
            ['declared', 's$ = "a"\nz$ = s$\n', []],
            ['declared-later', 'z$ = s$\ns$ = "a"\n', []],
            ['no-hint-without-case-match', 'Other$ = "a"\nz$ = s$\n', ['Variable s$ used before declaration']],
            ['in-quotes', 'z$ = "s$ and t$"\n', []],
            ['in-single-quotes', "z$ = 's$'\n", []],
            ['dir-token', 'p$ = $DIR_MACROS$\n', []],
            ['inside-hashes', 'z$ = #s$#\n', []],
            ['concat', 'a$ = "x"\nz$ = a$&b$\n', ['Variable b$ used before declaration']],
            ['substring', 'a$ = "xy"\nz$ = a$[1,1]\n', []],
            ['condition', 'if t$ = "a" then\n  z = 1\nendif\n', []],
            ['duplicate', 'z$ = s$&s$\n', ['Variable s$ used before declaration']],
            ['get-line-skipped', 'z$ = get(s$)\n', []],
            ['return-skipped', 'z$ = RETURNVAL$\n', []],
            // A string array element assignment declares the name.
            ['array-element', 'R0 = 1\nType$(R0) = "a"\nz$ = Type$(R0)\n', []],
            ['array-sized', 'core_t$() = 5470\nz$ = core_t$(1)\n', []],
        ];
        for (const [name, text, expected] of stringCases) {
            const result = await pullText(session, `used-before-string-${name}.prg`, text);
            assertEqual(inners(result.items), expected, `string variable: ${name}`);
        }

        const callHint = await pullText(
            session,
            'used-before-call-string-hint.prg',
            'Foo$ = "a"\ncall("f.prg", foo$)\n'
        );
        assertEqual(
            inners(callHint.items),
            ['Variable foo$ used before declaration (did you mean Foo$?)'],
            'call-arg string gets the case hint'
        );

        const callArg = await pullText(session, 'used-before-call-arg.prg', 'call("f.prg", undeclared)\n');
        assertEqual(
            inners(callArg.items),
            ['Variable undeclared used before declaration'],
            'call second arg is a ref'
        );

        const callBare = await pullText(session, 'used-before-call-bare.prg', 'call(undeclared)\n');
        assertEqual(inners(callBare.items), [], 'invalid path first arg is not a call-ref; generic skipped');

        const callAssign = await pullText(
            session,
            'used-before-call-assign.prg',
            'ret$ = call("f.prg", x)\n'
        );
        assertEqual(
            inners(callAssign.items),
            ['Variable x used before declaration'],
            'assignment prefix is not a call-ref'
        );

        const callString = await pullText(
            session,
            'used-before-call-string.prg',
            'foo$ = "a"\ncall("f.prg", foo$)\n'
        );
        assertEqual(inners(callString.items), [], 'call-arg $ strip uses stringvariablesSet');

        const callStringMissing = await pullText(
            session,
            'used-before-call-string-missing.prg',
            'call("f.prg", foo$)\n'
        );
        assertEqual(
            inners(callStringMissing.items),
            ['Variable foo$ used before declaration'],
            'undeclared call-arg keeps $'
        );

        const callExpr = await pullText(
            session,
            'used-before-call-expr.prg',
            'call("f.prg", a+b)\n'
        );
        assertEqual(
            inners(callExpr.items).sort(),
            ['Variable a used before declaration', 'Variable b used before declaration'].sort(),
            'call expression splits unique names'
        );

        const recall = await pullText(session, 'used-before-recall.prg', 'recall = missing\n');
        assertEqual(inners(recall.items), [], 'substring call skips generic');

        const forget = await pullText(session, 'used-before-forget.prg', 'forget(x)\n');
        assertEqual(inners(forget.items), [], 'forget( contains get( and skips generic');

        const upperCall = await pullText(session, 'used-before-upper-call.prg', 'CALL x\n');
        assertEqual(
            inners(upperCall.items),
            ['Variable CALL used before declaration', 'Variable x used before declaration'],
            'CALL substring is case-sensitive; generic still runs'
        );

        const dbcreate = await pullText(session, 'used-before-dbcreate.prg', 'dbcreate(handle)\n');
        assertEqual(
            inners(dbcreate.items),
            ['Variable handle used before declaration'],
            'dbcreate out-var'
        );

        const dbcreateNumeric = await pullText(
            session,
            'used-before-dbcreate-numeric.prg',
            'dbcreate(1)\n'
        );
        assertEqual(inners(dbcreateNumeric.items), [], 'numeric dbcreate id ignored');

        const dbcreateCont = await pullText(
            session,
            'used-before-dbcreate-cont.prg',
            'dbcreate(handle) INDEX \\\n  XPOS A 8 x\n'
        );
        assertEqual(
            inners(dbcreateCont.items),
            ['Variable handle used before declaration'],
            'dbcreate continuation fields ignored'
        );
        assertEqual(usedBeforeItems(dbcreateCont.items)[0].range.start.line, 0, 'dbcreate diagnostic on start line');

        const dbcreateDeclared = await pullText(
            session,
            'used-before-dbcreate-declared.prg',
            'handle = 1\ndbcreate(handle)\n'
        );
        assertEqual(inners(dbcreateDeclared.items), [], 'dbcreate out-var already in extract');

        const keywordSkip = await pullText(session, 'used-before-keyword.prg', 'z = askdim\n');
        assertEqual(inners(keywordSkip.items), [], 'keyword-catalog label skips undeclared');

        const longDeclared = await pullText(
            session,
            'used-before-length.prg',
            'VeryLongVariableNameX = 1\n'
        );
        assertEqual(
            inners(longDeclared.items),
            ['Keyword VeryLongVariableNameX exceeds maximum length of 16 characters'],
            '16-char only when name is in variablesSet'
        );
        assertEnvelope(
            usedBeforeItems(longDeclared.items)[0],
            'VeryLongVariableNameX = 1',
            'Keyword VeryLongVariableNameX exceeds maximum length of 16 characters',
            0,
            2
        );

        const longUndeclared = await pullText(
            session,
            'used-before-length-undeclared.prg',
            'z = VeryLongUndeclaredY\n'
        );
        assertEqual(
            inners(longUndeclared.items),
            ['Variable VeryLongUndeclaredY used before declaration'],
            'undeclared 17-char name is not a length error'
        );

        const duplicate = await pullText(session, 'used-before-duplicate.prg', 'z = missing + missing\n');
        assertEqual(
            inners(duplicate.items),
            ['Variable missing used before declaration'],
            'duplicate generic matches share message+line and collapse in the post-filter'
        );

        const returnSkip = await pullText(session, 'used-before-return.prg', 'z = RETURNVALUE\n');
        assertEqual(inners(returnSkip.items), [], 'RETURN substring skips generic identifier');

        const origSkip = await pullText(
            session,
            'used-before-orig.prg',
            'z = SHEETORIG\nw = AREAORIG\n'
        );
        assertEqual(inners(origSkip.items), [], 'SHEETORIG and AREAORIG skipped');

        const stringN = await pullText(session, 'used-before-n.prg', 'foo$ = N\n');
        assertEqual(inners(stringN.items), [], 'string-assign N skipped');

        const hashes = await pullText(session, 'used-before-hashes.prg', 'z = #GLOBAL#\n');
        assertEqual(inners(hashes.items), [], 'name between first and last # skipped');

        const tabAPath = path.join(FIXTURE, 'custom', 'used_before_tab_a.prg');
        const tabBPath = path.join(FIXTURE, 'custom', 'used_before_tab_b.prg');
        const uriA = pathToFileURL(tabAPath).href;
        const uriB = pathToFileURL(tabBPath).href;
        session.didOpen({
            uri: uriA,
            languageId: 'vertexadt',
            version: 1,
            text: fs.readFileSync(tabAPath, 'utf8'),
        });
        session.didOpen({
            uri: uriB,
            languageId: 'vertexadt',
            version: 1,
            text: fs.readFileSync(tabBPath, 'utf8'),
        });
        const pullA = await session.diagnostic(uriA);
        const pullB = await session.diagnostic(uriB);
        assertEqual(inners(pullA.items), [], 'tab A this-document extract');
        assertEqual(inners(pullB.items), [], 'tab B this-document extract');

        const leak = await pullText(session, 'used-before-other-tab.prg', 'z = onlyA\n');
        assertEqual(
            inners(leak.items),
            ['Variable onlyA used before declaration'],
            'other-tab names are not declared in this document'
        );

        const neighborhoodPath = path.join(FIXTURE, 'custom', 'used_before_neighborhood.prg');
        const neighborhoodUri = pathToFileURL(neighborhoodPath).href;
        session.didOpen({
            uri: neighborhoodUri,
            languageId: 'vertexadt',
            version: 1,
            text: fs.readFileSync(neighborhoodPath, 'utf8'),
        });
        await session.waitQuiet();
        const neighborhoodPull = await session.diagnostic(neighborhoodUri);
        assertEqual(
            inners(neighborhoodPull.items),
            [],
            'include-neighborhood extract names are declared'
        );

        // --- Used before assignment (vertexadt.variables.warnUsedBeforeAssignment) ---
        const assignmentCases = [
            ['ordered', 'a = 1\nb = a\n', []],
            ['self', 'x = x + 1\n', ['Variable x used before assignment (first assigned on line 1)']],
            ['self-later-line', 'x = 1\nx = x + 1\n', []],
            ['array-target', 'arr(1) = 5\nb = arr(1)\n', []],
            ['branch', 'if c > 0 then\n  a = 1\nendif\nb = a\n', []],
            ['loop', 'loop\n  b = a\n  a = 1\nendloop\n', ['Variable a used before assignment (first assigned on line 3)']],
            ['same-line-later', 'b = a\na = 2\nc = a\n', ['Variable a used before assignment (first assigned on line 2)']],
            [
                'other-section',
                '#A#\nx = 1\n#A#\n#B#\ny = x\n#B#\n',
                ['Variable x used before assignment (only assigned in section A, line 2)'],
            ],
            ['same-name-blocks', '#A#\nx = 1\n#A#\n#A#\ny = x\n#A#\n', []],
            ['file-level-before-section', 'x = 1\n#A#\ny = x\n#A#\n', []],
            [
                'section-then-file-level',
                '#A#\nx = 1\n#A#\ny = x\n',
                ['Variable x used before assignment (only assigned in section A, line 2)'],
            ],
            [
                'later-in-section-wins',
                '#A#\nx = 1\n#A#\n#B#\ny = x\nx = 2\n#B#\n',
                ['Variable x used before assignment (first assigned on line 6)'],
            ],
            ['call-arg', 'call("f.prg", v)\nv = 1\n', ['Variable v used before assignment (first assigned on line 2)']],
            ['call-arg-ordered', 'v = 1\ncall("f.prg", v)\n', []],
            [
                'call-self',
                'ret$ = call("f.prg", ret$)\n',
                ['Variable ret$ used before assignment (first assigned on line 1)'],
            ],
            ['dbcreate', 'dbcreate(handle)\nhandle = 1\n', ['Variable handle used before assignment (first assigned on line 2)']],
            ['synthetic', 'find_text_param(Obj) 12\nz = T12\nObj = 1\n', ['Variable Obj used before assignment (first assigned on line 3)']],
            // Only statement-start `X =` assigns: a comparison or text in a string does not.
            [
                'comparison-is-a-read',
                'if a = 1 then\n  b = 2\nendif\na = 3\n',
                ['Variable a used before assignment (first assigned on line 4)'],
            ],
            ['comparison-only', 'if a = 1 then\n  b = 2\nendif\nc = a\n', []],
            ['then-assigns', 'if c > 0 then a = 1\nb = a\n', []],
            ['string-text-only', 's$ = "where ACT = X"\nACT A 2 x\n', []],
            ['semicolon-statements', 'p = 1; x = 2\ny = x\n', []],
            ['semicolon-same-line', 'x = 2; y = x\n', []],
            [
                'semicolon-later-statement',
                'y = x; x = 2\n',
                ['Variable x used before assignment (first assigned on line 1)'],
            ],
            ['set-assigns', 'set DXL, 1\nz = DXL\n', []],
            [
                'array-read-before',
                'z = arr(1)\narr(1) = 2\n',
                ['Variable arr used before assignment (first assigned on line 2)'],
            ],
            ['call-is-not-assignment', 'z = foo(1)\n', []],
            // Numeric uses only look at numeric sites; `dx$ =` does not assign `dx`.
            ['string-site-only', 'dx$ = "a"\nz = dx\n', []],
            // `X$` uses look only at string sites; the target of `z$ =` is not a read.
            ['string-later', 'z$ = s$\ns$ = "a"\n', ['Variable s$ used before assignment (first assigned on line 2)']],
            ['string-ordered', 's$ = "a"\nz$ = s$\n', []],
            ['string-self', 's$ = s$&"a"\n', ['Variable s$ used before assignment (first assigned on line 1)']],
            ['string-semicolon', 's$ = "a"; z$ = s$\n', []],
            [
                'call-string-later',
                'call("f.prg", s$)\ns$ = "a"\n',
                ['Variable s$ used before assignment (first assigned on line 2)'],
            ],
        ];
        for (const [name, text, expected] of assignmentCases) {
            const result = await pullText(session, `used-before-assign-${name}.prg`, text);
            assertEqual(assignmentInners(result.items), expected, `used before assignment: ${name}`);
        }

        const neighborhoodAssign = await session.diagnostic(neighborhoodUri);
        assertEqual(assignmentInners(neighborhoodAssign.items), [], 'neighborhood names are assigned before line 1');

        session.didChangeConfiguration({ variables: { warnUsedBeforeAssignment: false } });
        await session.waitQuiet(200);
        const settingOff = await pullText(session, 'used-before-assign-off.prg', 'z = later\nlater = 1\n');
        assertEqual(assignmentInners(settingOff.items), [], 'setting off skips used before assignment');
        assertEqual(inners(settingOff.items), [], 'setting off keeps the declaration check quiet');
        const settingOffUndeclared = await pullText(session, 'used-before-assign-off-undeclared.prg', 'z = missing\n');
        assertEqual(
            inners(settingOffUndeclared.items),
            ['Variable missing used before declaration'],
            'setting off keeps used before declaration'
        );

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    run()
        .then(() => {
            console.log('16-used-before goldens passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run };
