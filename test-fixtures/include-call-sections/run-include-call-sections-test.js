'use strict';

const support = require('../../server/out/targetSectionSupport');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, message) {
    const actualText = JSON.stringify(actual);
    const expectedText = JSON.stringify(expected);
    assert(actualText === expectedText, `${message}: expected ${expectedText}, got ${actualText}`);
}

const calleeText = [
    '#JOINT#',
    'x = 1',
    '#JOINT#',
    '#MACROS#',
    'y = 2',
    '#MACROS#',
    '#MACROS#',
    'z = 3',
    '#MACROS#',
    '#SECTION#',
    'ignored = 4',
    '#SECTION#'
].join('\n');

const names = support.listUniqueTargetSectionNames(calleeText);
assertEqual(names, ['JOINT', 'MACROS'], 'unique alphabetical names skip keyword collisions and duplicates');

assert(!support.isUnknownTargetSectionName('MACROS', names), 'exact name is known');
assert(!support.isUnknownTargetSectionName('MAC', names), 'prefix of a real name is not unknown');
assert(!support.isUnknownTargetSectionName('SECTIONOFFILE', names), 'keyword is not unknown');
assert(!support.isUnknownTargetSectionName('section', names), 'keyword is not unknown, case-insensitive');
assert(support.isUnknownTargetSectionName('MACROX', names), 'typo that is not a prefix is unknown');
assert(support.isUnknownTargetSectionName('JOINTS', names), 'longer mismatch is unknown');
assert(support.isUnknownTargetSectionName('MACROS', []), 'name on a file with no sections is unknown');
assert(!support.isUnknownTargetSectionName('', names), 'empty token is not unknown');

const includeLine = '# include callee.prg MACROS';
const includeSites = support.collectNamedSectionSuffixes(includeLine);
assert(includeSites.length === 1, 'include site count');
assertEqual(includeSites[0].filePath, 'callee.prg', 'include file path');
assertEqual(includeSites[0].targetSection, 'MACROS', 'include section');
assertEqual(includeLine.slice(includeSites[0].sectionStart, includeSites[0].sectionEnd), 'MACROS', 'include underline token');

const quotedInclude = '# include "callee.prg" JOINT';
const quotedSites = support.collectNamedSectionSuffixes(quotedInclude);
assertEqual(quotedInclude.slice(quotedSites[0].sectionStart, quotedSites[0].sectionEnd), 'JOINT', 'quoted include section is after quotes');

const insideQuotedInclude = '# include "callee.prg JOINT"';
const insideQuotedSites = support.collectNamedSectionSuffixes(insideQuotedInclude);
assertEqual(insideQuotedInclude.slice(insideQuotedSites[0].sectionStart, insideQuotedSites[0].sectionEnd), 'JOINT', 'section inside quoted include path');

const callParen = 'Ret$=call("callee.prg MACROS",Area)';
const callSites = support.collectNamedSectionSuffixes(callParen);
assert(callSites.length === 1, 'paren call site count');
assertEqual(callParen.slice(callSites[0].sectionStart, callSites[0].sectionEnd), 'MACROS', 'paren call underline token');
assertEqual(callSites[0].form, 'call_paren', 'paren call form');

const secondArg = 'call("callee.prg", MACROS, Area)';
const secondArgSites = support.collectNamedSectionSuffixes(secondArg);
assert(secondArgSites.length === 0, 'second-argument form is not a named section suffix');

const spaceCall = 'call callee.prg JOINT';
const spaceSites = support.collectNamedSectionSuffixes(spaceCall);
assert(spaceSites.length === 1, 'space call site count');
assertEqual(spaceCall.slice(spaceSites[0].sectionStart, spaceSites[0].sectionEnd), 'JOINT', 'space call underline token');
assertEqual(spaceSites[0].form, 'call_space', 'space call form');

const keywordInclude = '# include callee.prg SECTIONOFFILE';
assertEqual(support.collectNamedSectionSuffixes(keywordInclude), [], 'SECTIONOFFILE is not a named suffix');

function statusFor(filePath) {
    if (filePath === 'callee.prg') {
        return { isFile: true, isDirectory: false };
    }
    if (filePath === 'tools') {
        return { isFile: false, isDirectory: true };
    }
    if (filePath === 'both') {
        return { isFile: true, isDirectory: true };
    }
    if (filePath === 'plain.prg') {
        return { isFile: true, isDirectory: false };
    }
    return { isFile: false, isDirectory: false };
}

function namesFor(filePath) {
    if (filePath === 'callee.prg' || filePath === 'both') {
        return names;
    }
    return [];
}

function analyze(lineText, cursorCharacter, triggerKind) {
    return support.analyzeIncludeCallCompletion({
        lineText,
        cursorCharacter,
        triggerKind,
        pathStatus: statusFor,
        getSectionNames: namesFor
    });
}

const includeSpace = '# include callee.prg ';
const spaceResult = analyze(includeSpace, includeSpace.length, 2);
assertEqual(spaceResult.mode, 'sections', 'space after file opens sections');
assertEqual(spaceResult.sections, names, 'section list');
assertEqual(spaceResult.insertLeadingSpace, false, 'space already present');

const includeNoSpace = '# include callee.prg';
const invokeAtPath = analyze(includeNoSpace, includeNoSpace.length, support.TRIGGER_INVOKED);
assertEqual(invokeAtPath.mode, 'sections', 'Ctrl+Space at end of file path opens sections');
assertEqual(invokeAtPath.insertLeadingSpace, true, 'Ctrl+Space inserts a leading space');

const typingPath = analyze(includeNoSpace, includeNoSpace.length, 2);
assertEqual(typingPath.mode, 'paths', 'typing the filename does not force the section picker');

const partial = '# include callee.prg MA';
const partialResult = analyze(partial, partial.length, 2);
assertEqual(partialResult.mode, 'sections', 'partial suffix stays in the slot');
assertEqual(partial.slice(partialResult.replaceStart, partialResult.replaceEnd), 'MA', 'partial suffix replace range');

const completeSuffix = '# include callee.prg MACROS';
const completeResult = analyze(completeSuffix, completeSuffix.length, 2);
assertEqual(completeResult.mode, 'sections', 'cursor at end of complete suffix still replaces it');
assertEqual(completeSuffix.slice(completeResult.replaceStart, completeResult.replaceEnd), 'MACROS', 'complete suffix replace range');

const pastSuffix = '# include callee.prg MACROS ';
const pastResult = analyze(pastSuffix, pastSuffix.length, 2);
assertEqual(pastResult.mode, 'none', 'cursor past the suffix slot suggests nothing');

const keywordReplace = '# include callee.prg SECTIONOFFILE';
const keywordResult = analyze(keywordReplace, keywordReplace.length, support.TRIGGER_INVOKED);
assertEqual(keywordResult.mode, 'sections', 'existing SECTIONOFFILE token can be replaced');
assertEqual(keywordReplace.slice(keywordResult.replaceStart, keywordResult.replaceEnd), 'SECTIONOFFILE', 'keyword replace range');

const quotedCall = 'call("callee.prg ';
const quotedCallResult = analyze(quotedCall, quotedCall.length, 2);
assertEqual(quotedCallResult.mode, 'sections', 'space inside first call string opens sections');
assertEqual(quotedCallResult.insertLeadingSpace, false, 'space inside quotes already present');

const quotedCallPath = 'call("callee.prg")';
const afterQuote = analyze(quotedCallPath, quotedCallPath.length - 1, support.TRIGGER_INVOKED);
assertEqual(afterQuote.mode, 'none', 'cursor after closing quote of call() suggests nothing');

const assignCall = 'Ret$=call("callee.prg';
const assignInvoke = analyze(assignCall, assignCall.length, support.TRIGGER_INVOKED);
assertEqual(assignInvoke.mode, 'sections', 'assignment call form is in scope');
assertEqual(assignInvoke.insertLeadingSpace, true, 'assignment call inserts space inside quotes');

const spaceForm = 'call callee.prg ';
const spaceFormResult = analyze(spaceForm, spaceForm.length, 2);
assertEqual(spaceFormResult.mode, 'sections', 'space-form call opens sections');

const dirSpace = '# include tools ';
const dirResult = analyze(dirSpace, dirSpace.length, 2);
assertEqual(dirResult.mode, 'none', 'space after a directory does not open sections');

const bothSpace = '# include both ';
const bothResult = analyze(bothSpace, bothSpace.length, 2);
assertEqual(bothResult.mode, 'none', 'directory wins when a path is both file and folder');

const noSectionFile = '# include plain.prg ';
const noSectionResult = analyze(noSectionFile, noSectionFile.length, 2);
assertEqual(noSectionResult.mode, 'none', 'no picker when the file has no sections');

const includedFrom = '##included_from callee.prg ';
const includedResult = analyze(includedFrom, includedFrom.length, 2);
assertEqual(includedResult.mode, 'paths', 'included_from never switches to sections');

const unknownLine = '# include callee.prg MACROX';
const unknownSites = support.collectNamedSectionSuffixes(unknownLine);
assert(unknownSites.length === 1, 'unknown suffix is still a named section site');
assert(support.isUnknownTargetSectionName(unknownSites[0].targetSection, names), 'MACROX warns');
assertEqual(
    `Unknown section '${unknownSites[0].targetSection}' in callee.prg`,
    "Unknown section 'MACROX' in callee.prg",
    'warning message shape'
);
assertEqual(unknownLine.slice(unknownSites[0].sectionStart, unknownSites[0].sectionEnd), 'MACROX', 'warning underlines only the suffix');

const prefixLine = '# include callee.prg MAC';
const prefixSites = support.collectNamedSectionSuffixes(prefixLine);
assert(!support.isUnknownTargetSectionName(prefixSites[0].targetSection, names), 'MAC does not warn while it is a prefix');

const emptyInclude = '# include ';
const emptyResult = analyze(emptyInclude, emptyInclude.length, 2);
assertEqual(emptyResult.mode, 'paths', 'empty include path lists roots');
assertEqual(emptyResult.filePath, '', 'empty path query');

assertEqual(
    support.lookupPathFromCallArgument('callee.prg MACROS'),
    'callee.prg',
    'parameter lookup strips a named section suffix'
);
assertEqual(
    support.lookupPathFromCallArgument('"$DIR_MACROS/foo.prg SECTION"'),
    '$DIR_MACROS/foo.prg',
    'parameter lookup strips SECTION keyword from a quoted call path'
);
assertEqual(
    support.lookupPathFromCallArgument('callee.prg'),
    'callee.prg',
    'parameter lookup leaves a plain path alone'
);

assert(support.isInParenthesizedCallArgumentList('Ret$=call("callee.prg",', 'Ret$=call("callee.prg",'.length), 'cursor after comma is in the parameter list');
assert(support.isInParenthesizedCallArgumentList('call("callee.prg", Area', 'call("callee.prg", Area'.length), 'bare call() parameter list');
assert(!support.isInParenthesizedCallArgumentList('Ret$=call("callee.prg', 'Ret$=call("callee.prg'.length), 'cursor in the first argument is not a parameter slot');
assert(!support.isInParenthesizedCallArgumentList('# include callee.prg MACROS', '# include callee.prg MACROS'.length), 'include suffix is not a call parameter list');

const assignComma = 'Ret$=call("callee.prg",';
assertEqual(analyze(assignComma, assignComma.length, 2).mode, 'none', 'assignment call parameters do not open path/section completion');
const sectionThenParams = 'Ret$=call("callee.prg MACROS",';
assertEqual(analyze(sectionThenParams, sectionThenParams.length, 2).mode, 'none', 'section inside the first argument does not steal later parameters');

const sectionalCallee = [
    '#OTHER#',
    'a = get($CALLPARAM1)',
    'b = get($CALLPARAM2)',
    '#OTHER#',
    '#SECTION1#',
    'keep = 1',
    '#SECTION1#'
].join('\n');
assertEqual(
    support.getExpectedCallParamCountFromText(sectionalCallee),
    2,
    'whole-file call still sees both $CALLPARAM slots'
);
assertEqual(
    support.getExpectedCallParamCountFromText(sectionalCallee, 'SECTION1'),
    0,
    'call("file.prg SECTION1") does not inherit $CALLPARAM from other sections'
);
assertEqual(
    support.getExpectedCallParamCountFromText(sectionalCallee, 'OTHER'),
    2,
    'call("file.prg OTHER") uses that section’s $CALLPARAM count'
);

const quotedIncludeSpace = '# include "callee.prg" ';
const quotedIncludeResult = analyze(quotedIncludeSpace, quotedIncludeSpace.length, 2);
assertEqual(quotedIncludeResult.mode, 'sections', 'quoted include section is after the closing quote');

console.log('include-call-sections tests passed');
