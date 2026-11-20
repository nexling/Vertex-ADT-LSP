'use strict';

const path = require('path');
const support = require('../../server/out/formNameCompletion');

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

function analyze(line, cursor) {
    const at = typeof cursor === 'number' ? cursor : line.length;
    return support.analyzeFormNameCompletion(line, at);
}

function expectSlot(line, cursor, insideQuotes, replaceStart, replaceEnd, message) {
    const result = analyze(line, cursor);
    assert(result, `${message}: expected a form slot`);
    assertEqual(result.insideQuotes, insideQuotes, `${message}: insideQuotes`);
    if (typeof replaceStart === 'number') {
        assertEqual(result.replaceStart, replaceStart, `${message}: replaceStart`);
    }
    if (typeof replaceEnd === 'number') {
        assertEqual(result.replaceEnd, replaceEnd, `${message}: replaceEnd`);
    }
}

function expectNoSlot(line, cursor, message) {
    const result = analyze(line, cursor);
    assert(result === null, `${message}: expected no form slot, got ${JSON.stringify(result)}`);
}

expectSlot('edit_obj_data(Obj,', undefined, false, undefined, undefined, 'edit_obj_data after comma');
expectSlot('ok = edit_jnt_data(conn, ', undefined, false, undefined, undefined, 'edit_jnt_data after comma and space');
expectSlot('dbedit(1,', undefined, false, undefined, undefined, 'dbedit after comma');
expectSlot('set_macro_form(Mac)', undefined, false, undefined, undefined, 'set_macro_form after close paren');
expectSlot('set_macro_form(Mac) ', undefined, false, undefined, undefined, 'set_macro_form after space');

const quotedObj = 'edit_obj_data(Obj,"form';
expectSlot(quotedObj, quotedObj.length, true, quotedObj.indexOf('"') + 1, quotedObj.length, 'edit_obj_data inside open quotes');

const closedObj = 'edit_obj_data(Obj,"form.vxf",0)';
const formStart = closedObj.indexOf('"') + 1;
const formEnd = closedObj.indexOf('"', formStart);
expectSlot(closedObj, formStart + 4, true, formStart, formEnd, 'edit_obj_data replace existing quoted token');

const quotedMacro = 'set_macro_form(Mac) "form';
expectSlot(quotedMacro, quotedMacro.length, true, quotedMacro.lastIndexOf('"') + 1, quotedMacro.length, 'set_macro_form inside open quotes');

const singleQuoted = "dbedit(1,'form";
expectSlot(singleQuoted, singleQuoted.length, true, singleQuoted.indexOf("'") + 1, singleQuoted.length, 'dbedit inside single quotes');

const nested = 'edit_obj_data(foo(a,b), "';
expectSlot(nested, nested.length, true, nested.lastIndexOf('"') + 1, nested.length, 'nested first-arg parens still find param 2');

expectSlot('EDIT_OBJ_DATA(Obj, "', undefined, true, undefined, undefined, 'case-insensitive command name');

expectNoSlot('edit_obj_data(Obj', undefined, 'still on first inside parameter');
expectNoSlot('edit_obj_data(', undefined, 'just opened paren is param 1');
expectNoSlot('edit_obj_data(Obj, Form$', undefined, 'unquoted identifier is not a string literal');
expectNoSlot('edit_obj_data(Obj, "form.vxf", ', undefined, 'third parameter is not a form slot');
expectNoSlot('edit_jnt_data(conn,"form.vxf")', undefined, 'after closed call is not a form slot');
expectNoSlot('set_macro_form(Mac', undefined, 'set_macro_form still inside parens');
expectNoSlot('set_macro_form(Mac) "form.vxf"', undefined, 'after closed outside string is not a form slot');
expectNoSlot('dbeditfrm 1, "', undefined, 'dbeditfrm is not one of the four commands');
expectNoSlot('new_macro("LIB"', undefined, 'unrelated command');
expectNoSlot('edit_obj_data(Obj, "x") ; comment', 'edit_obj_data(Obj, "x") ; comment'.length, 'cursor in comment');

const insertQuoted = support.formInsertText('alpha.vxf', false);
assertEqual(insertQuoted, '"alpha.vxf"', 'insert with wrapping quotes');
assertEqual(support.formInsertText('alpha.vxf', true), 'alpha.vxf', 'insert inside existing quotes');

const fixtureRoot = __dirname;
const customForms = path.join(fixtureRoot, 'custom', 'forms');
const systemForms = path.join(fixtureRoot, 'system', 'forms');
const entries = support.collectFormNameEntries(customForms, systemForms);
const names = entries.map(entry => entry.name);
const origins = {};
for (const entry of entries) {
    origins[entry.name] = entry.origin;
}

assert(names.indexOf('nested') === -1, 'directory names are not suggested');
assert(names.indexOf('skip.vxf') === -1, 'files in subfolders are not suggested');
assert(names.indexOf('notes.txt') !== -1, 'non-vxf files directly in forms/ are suggested');
assert(names.indexOf('alpha.vxf') !== -1, 'custom file is suggested');
assert(names.indexOf('system_only.vxf') !== -1, 'system-only file is suggested');
assertEqual(origins['shared.vxf'], 'custom/forms', 'custom wins on duplicate filenames');
assertEqual(origins['alpha.vxf'], 'custom/forms', 'custom origin label');
assertEqual(origins['system_only.vxf'], 'system/forms', 'system origin label');
assertEqual(names, names.slice().sort((a, b) => a.localeCompare(b, undefined, { sensitivity: 'base' })), 'merged list is alphabetical');

const workspaceEntries = support.collectWorkspaceFormNames({
    workspaceRoot: fixtureRoot,
    customName: 'custom'
});
assertEqual(
    workspaceEntries.map(entry => entry.name),
    names,
    'workspace resolver finds custom/forms and system/forms from the Vertex root'
);

console.log('form-name-completion tests passed');
