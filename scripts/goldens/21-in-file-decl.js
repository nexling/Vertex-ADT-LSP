'use strict';

/**
 * Parity goldens for in-file declaration-number duplicates (family 10)
 * and redeclaration (family 11). Assertions come from the cited TESTING.md
 * scenarios plus uniquely named extras.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '..', '..');
const DECL_DIR = path.join(ROOT, 'test-fixtures', 'workspace-declaration-index');
const SLOT_DIR = path.join(ROOT, 'test-fixtures', 'local-slot-names');
const FORM_DIR = path.join(ROOT, 'test-fixtures', 'verification-ignored-sections');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

function readText(filePath) {
    return fs.readFileSync(filePath, 'utf8');
}

function lineAt(text, line) {
    return text.split('\n')[line];
}

function wrapped(lineText, inner) {
    return `${lineText} has an error: ${inner}`;
}

function itemsOf(pull) {
    return (pull && pull.items) || [];
}

function openAndPull(session, filePath, text) {
    const uri = pathToFileURL(filePath).href;
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    return { uri, text };
}

function findMessage(items, needle) {
    return items.find((item) => typeof item.message === 'string' && item.message.includes(needle));
}

function assertWarning(diag, label, extra) {
    assert(diag, `missing diagnostic for ${label}`);
    assertEqual(diag.severity, 2, `${label} severity`);
    assertEqual(diag.source, 'ADT-Syntax', `${label} source`);
    if (extra && extra.expectedMessage) {
        assertEqual(diag.message, extra.expectedMessage, `${label} message`);
    }
    if (extra && Object.prototype.hasOwnProperty.call(extra, 'code') && extra.code === undefined) {
        assert(diag.code === undefined || diag.code === null, `${label} has no code`);
    } else if (extra && extra.code) {
        assertEqual(diag.code, extra.code, `${label} code`);
    }
    if (extra && extra.related) {
        const related = (diag.relatedInformation || [])[0];
        assert(related, `${label} relatedInformation`);
        assertEqual(related.message, extra.related, `${label} related message`);
        if (extra.relatedLine !== undefined) {
            assertEqual(
                related.location && related.location.range && related.location.range.start.line,
                extra.relatedLine,
                `${label} related line`
            );
        }
    }
}

async function run(session) {
    await session.initialize();

    const macrosPath = path.join(DECL_DIR, 'custom', 'section_macros.prg');
    const macrosText = readText(macrosPath);
    const macros = openAndPull(session, macrosPath, macrosText);
    const macrosItems = itemsOf(await session.diagnostic(macros.uri));
    assertEqual(
        macrosItems.filter((item) => typeof item.message === 'string' && item.message.includes('declaration number')).length,
        0,
        'section_macros.prg has no in-file duplicate'
    );

    const dupPath = path.join(DECL_DIR, 'custom', 'section_dup.prg');
    const dupText = readText(dupPath);
    const dup = openAndPull(session, dupPath, dupText);
    const dupItems = itemsOf(await session.diagnostic(dup.uri));
    const dupLine = 3;
    const dupInner = 'Variable var2 has declaration number 1, which is already used by var1 in this section.';
    const dupDiag = findMessage(dupItems, 'already used by var1');
    assertWarning(dupDiag, 'section_dup.prg var2', {
        expectedMessage: wrapped(lineAt(dupText, dupLine), dupInner),
        code: undefined,
        related: dupInner,
    });
    assertEqual(dupDiag.range.start.line, dupLine, 'section_dup.prg warning line');
    assertEqual(
        dupItems.filter((item) => typeof item.message === 'string' && item.message.includes('already used by')).length,
        1,
        'section_dup.prg one duplicate'
    );

    const multiPath = path.join(DECL_DIR, 'custom', 'section_multi_block_dup.prg');
    const multiText = readText(multiPath);
    const multi = openAndPull(session, multiPath, multiText);
    const multiItems = itemsOf(await session.diagnostic(multi.uri));
    const dVsPp = findMessage(multiItems, 'already used by PP');
    const otherVsD = findMessage(multiItems, 'already used by D');
    assertWarning(dVsPp, 'SECTION_B D vs PP', {
        expectedMessage: wrapped(
            lineAt(multiText, 10),
            'Variable D has declaration number 5, which is already used by PP in this section.'
        ),
        code: undefined,
        related: 'Variable D has declaration number 5, which is already used by PP in this section.',
    });
    assertEqual(dVsPp.range.start.line, 10, 'SECTION_B D=5 line');
    assertWarning(otherVsD, 'SECTION_A Other vs D', {
        expectedMessage: wrapped(
            lineAt(multiText, 16),
            'Variable Other has declaration number 5, which is already used by D in this section.'
        ),
        code: undefined,
        related: 'Variable Other has declaration number 5, which is already used by D in this section.',
    });
    assertEqual(otherVsD.range.start.line, 16, 'SECTION_A Other=5 line');
    assert(
        !findMessage(multiItems, 'already used by') ||
            multiItems.filter((item) => typeof item.message === 'string' && item.message.includes('already used by')).length === 2,
        'section_multi_block_dup.prg has exactly two duplicates'
    );
    assert(
        !multiItems.some((item) => typeof item.message === 'string' && item.message.includes('PP') && item.message.includes('SECTION_A')),
        'PP=22 in SECTION_A does not clash with SECTION_B'
    );

    const redeclPath = path.join(SLOT_DIR, 'custom', 'redeclare.prg');
    const redeclText = readText(redeclPath);
    const redecl = openAndPull(session, redeclPath, redeclText);
    const redeclItems = itemsOf(await session.diagnostic(redecl.uri));
    const redeclInner =
        'Variable A was declared as 32, now redeclared as 50; slot 32 is no longer bound to A';
    const redeclDiag = findMessage(redeclItems, 'declaration-redeclared')
        || findMessage(redeclItems, 'now redeclared as 50');
    assertWarning(redeclDiag, 'redeclare.prg A=50', {
        expectedMessage: wrapped(lineAt(redeclText, 2), redeclInner),
        code: 'vertexadt/declaration-redeclared',
        related: 'First declaration: A = 32',
        relatedLine: 1,
    });
    assertEqual(redeclDiag.range.start.line, 2, 'redeclare.prg warning line');
    assertEqual(
        redeclItems.filter((item) => item.code === 'vertexadt/declaration-redeclared').length,
        1,
        'redeclare.prg one redeclaration'
    );

    const formPath = path.join(FORM_DIR, 'custom', 'edit_form_ignored.prg');
    const formText = readText(formPath);
    const form = openAndPull(session, formPath, formText);
    const formItems = itemsOf(await session.diagnostic(form.uri));
    const formBodyLines = new Set([6, 7, 8, 9, 10, 11]);
    assert(
        !formItems.some((item) => formBodyLines.has(item.range && item.range.start && item.range.start.line)),
        'edit_form_ignored.prg has no diagnostics in #EDIT_FORM# body'
    );
    assert(!findMessage(formItems, 'DUPA'), 'no DUPA diagnostic');
    assert(!findMessage(formItems, 'DUPB'), 'no DUPB diagnostic');

    const extraPath = path.join(DECL_DIR, 'custom', 'in_file_decl_nn21.prg');
    const extraText = readText(extraPath);
    const extra = openAndPull(session, extraPath, extraText);
    const extraItems = itemsOf(await session.diagnostic(extra.uri));
    assert(!findMessage(extraItems, 'already used by Same'), 'same name + same number is allowed');
    assert(!findMessage(extraItems, 'ValueSlot'), 'value-typed row does not occupy');
    assert(!findMessage(extraItems, 'already used by ValueSlot'), 'Occupied=30 does not clash with value-typed 30');
    const bangDup = findMessage(extraItems, 'already used by BangKeep');
    assertWarning(bangDup, 'bang fragment still leaves the assignment row', {
        expectedMessage: wrapped(
            lineAt(extraText, 6),
            'Variable BangLater has declaration number 7, which is already used by BangKeep in this section.'
        ),
        code: undefined,
    });
    const localDup = findMessage(extraItems, 'already used by l1Area');
    assertWarning(localDup, 'local-slot names are ordinary names', {
        expectedMessage: wrapped(
            lineAt(extraText, 8),
            'Variable l1Other has declaration number 2, which is already used by l1Area in this section.'
        ),
        code: undefined,
    });
    assert(!findMessage(extraItems, 'Name$'), '$ suffix is not a declaration row');
    assert(!findMessage(extraItems, 'already used by Name'), 'Plain=3 does not clash with Name$');
    const bDup = findMessage(extraItems, 'already used by A');
    assertWarning(bDup, 'A=40 then B=40', {
        expectedMessage: wrapped(
            lineAt(extraText, 12),
            'Variable B has declaration number 40, which is already used by A in this section.'
        ),
        code: undefined,
    });
    const aRedecl = extraItems.find((item) => item.code === 'vertexadt/declaration-redeclared');
    assertWarning(aRedecl, 'A=40 then A=50 redeclaration', {
        expectedMessage: wrapped(
            lineAt(extraText, 13),
            'Variable A was declared as 40, now redeclared as 50; slot 40 is no longer bound to A'
        ),
        code: 'vertexadt/declaration-redeclared',
        related: 'First declaration: A = 40',
        relatedLine: 11,
    });
    assert(
        !extraItems.some((item) => item.code === 'vertexadt/declaration-redeclared' && item.message.includes('Cross')),
        'Cross=8 then Cross=9 across blocks is not a redeclaration'
    );

    session.didChangeConfiguration({
        locals: { warnOnRedeclaration: false },
    });
    const redeclOff = itemsOf(await session.diagnostic(redecl.uri));
    assertEqual(
        redeclOff.filter((item) => item.code === 'vertexadt/declaration-redeclared').length,
        0,
        'warnOnRedeclaration false suppresses family 11'
    );

    session.didChangeConfiguration({
        locals: { warnOnRedeclaration: true },
        diagnostics: { maxNumberOfProblems: 1 },
    });
    const capUri = pathToFileURL(path.join(DECL_DIR, 'custom', '.in_file_decl_nn21_cap.prg')).href;
    const capText = '@declarationnumbercheckstart\nA = 10\nB = 10\nA = 20\n@declarationnumbercheckend\n';
    session.didOpen({ uri: capUri, languageId: 'vertexadt', version: 1, text: capText });
    const capItems = itemsOf(await session.diagnostic(capUri));
    assert(
        capItems.some((item) => typeof item.message === 'string' && item.message.includes('already used by A')),
        'family 10 still emits at cap'
    );
    assertEqual(
        capItems.filter((item) => item.code === 'vertexadt/declaration-redeclared').length,
        0,
        'family 10 can starve family 11 at cap'
    );

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
            console.log('21-in-file-decl goldens passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
