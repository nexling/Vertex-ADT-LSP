#!/usr/bin/env node
'use strict';

/**
 * Regenerate the committed C++ under server/src/generated/ from
 * server/catalog/*.json. The JSON catalogs are the hand-maintained source of
 * truth: edit them, then run `npm run generate-catalogs` and commit both.
 * CMake does not run this script.
 *
 * Until 2026-11-20 the JSON was harvested from the Node language server with
 * native overrides layered on top (ADRs 0003, 0011, 0016). Node was removed
 * (ADR 0018); the overrides are now part of the JSON.
 */

const fs = require('fs');
const path = require('path');

const CUSTOM_VALIDATORS = Object.freeze([
    'abs',
    'dbcreate',
    'get_area_param',
    'new_iterator',
    'pnt_to_plane',
]);

const ROOT = path.resolve(__dirname, '..');
const CATALOG_DIR = path.join(ROOT, 'server', 'catalog');
const GENERATED_DIR = path.join(ROOT, 'server', 'src', 'generated');
const GENERATED_BANNER = [
    '// GENERATED FILE — do not hand-edit.',
    '// Regenerated from server/catalog/*.json by: npm run generate-catalogs',
    '',
].join('\n');

function readJson(name) {
    return JSON.parse(fs.readFileSync(path.join(CATALOG_DIR, name), 'utf8'));
}

function main() {
    const catalogs = {
        commands: readJson('commands.json'),
        keywords: readJson('keywords.json'),
        commandTypeWrites: readJson('command-type-writes.json'),
        objectParameters: readJson('object-parameters.json'),
    };
    assertCatalogs(catalogs);
    writeGeneratedCpp(catalogs);
    console.log(summarize(catalogs));
}

function assertCatalogs(catalogs) {
    const objectParameterCommands = Object.keys(catalogs.objectParameters).join(',');
    if (objectParameterCommands !== 'get_obj_param,get_material_param,get_area_param,get_section_param') {
        throw new Error(`Unexpected object-parameter commands: ${objectParameterCommands}`);
    }
    const names = Object.keys(catalogs.commands);
    if (names.length !== 298) {
        throw new Error(`Expected 298 command keys, got ${names.length}`);
    }
    if (names.includes('call')) {
        throw new Error('call must not be a command-catalog key');
    }
    const custom = names.filter((name) => catalogs.commands[name].customValidator);
    const table = names.filter((name) => catalogs.commands[name].tableValidator);
    if (custom.length !== 5) {
        throw new Error(`Expected 5 custom validators, got ${custom.join(', ')}`);
    }
    if (table.length !== 293) {
        throw new Error(`Expected 293 table validators, got ${table.length}`);
    }
    for (const name of CUSTOM_VALIDATORS) {
        const row = catalogs.commands[name];
        if (row.customValidator !== name || row.tableValidator) {
            throw new Error(`${name} must be customValidator only`);
        }
    }
    for (const name of table) {
        const row = catalogs.commands[name];
        if (row.customValidator) {
            throw new Error(`${name} has both validators`);
        }
    }

    const unbounded = names.filter((name) => {
        const s = catalogs.commands[name].structure;
        return s.maxParamsInside === 'unbounded' || s.maxParamsOutside === 'unbounded';
    });
    const expectedUnbounded = ['set_sheet_shape', 'new_feature', 'add_aux_task', 'new_ncaccessory'];
    if (unbounded.sort().join(',') !== expectedUnbounded.slice().sort().join(',')) {
        throw new Error(`Unexpected unbounded arity: ${unbounded.join(', ')}`);
    }

    const labels = catalogs.keywords.map((k) => k.label);
    for (const dup of ['ddrevid_last', 'fileinfo_to_meta']) {
        if (labels.filter((label) => label === dup).length < 2) {
            throw new Error(`Keyword catalog lost duplicate label ${dup}`);
        }
    }

    const writes = catalogs.commandTypeWrites;
    for (const key of ['Xlimit_max', 'Xlimit_min', 'Ylimit_max', 'Ylimit_min']) {
        if (!Object.prototype.hasOwnProperty.call(writes.returnTypeMap, key)
            && !hasMixedCase(writes, key)) {
            // Ticket 41: mixed-case keys live on the maps Node wrote. Probe which map.
        }
    }
    if (!hasMixedCase(writes, 'Xlimit_max')) {
        throw new Error('command-type-writes lost mixed-case Xlimit_max');
    }
    if (!writes.paramTypeMap.get_edge_dir || writes.paramTypeMap.get_edge_dir['1'] !== 'pvector') {
        throw new Error('get_edge_dir slot 1 missing');
    }
    if (!writes.paramTypeMap.get_edge_dir || writes.paramTypeMap.get_edge_dir['2'] !== 'dvector') {
        throw new Error('get_edge_dir slot 2 missing');
    }
}

function hasMixedCase(writes, key) {
    return ['returnTypeMap', 'inputParamTypeMap', 'paramTypeMap', 'multiParamTypeMap']
        .some((mapName) => Object.prototype.hasOwnProperty.call(writes[mapName], key));
}

function summarize(catalogs) {
    const names = Object.keys(catalogs.commands);
    const custom = names.filter((name) => catalogs.commands[name].customValidator);
    return [
        `Read ${names.length} commands (${names.length - custom.length} table, ${custom.length} custom),`,
        `${catalogs.keywords.length} keyword-catalog entries,`,
        `command-type-writes maps ${Object.keys(catalogs.commandTypeWrites.returnTypeMap).length}/` +
            `${Object.keys(catalogs.commandTypeWrites.inputParamTypeMap).length}/` +
            `${Object.keys(catalogs.commandTypeWrites.paramTypeMap).length}/` +
            `${Object.keys(catalogs.commandTypeWrites.multiParamTypeMap).length}`,
        `keys, ${Object.keys(catalogs.objectParameters).length} object-parameter commands.`,
        `Wrote server/src/generated/{catalog_data.h,command_catalog_data.cpp,keyword_catalog_data.cpp,command_type_writes_data.cpp,object_parameters_data.cpp}.`,
    ].join(' ');
}

const SLOT_KIND = Object.freeze({
    'normal variable': 'SlotKind::NormalVariable',
    'string variable': 'SlotKind::StringVariable',
    'any variable': 'SlotKind::AnyVariable',
    'string expression': 'SlotKind::StringExpression',
    'string-or-variable': 'SlotKind::StringOrVariable',
    'number-or-variable': 'SlotKind::NumberOrVariable',
    'digit literal or variable': 'SlotKind::DigitLiteralOrVariable',
});

const VARIABLE_KIND = Object.freeze({
    'any variable': 'VariableKind::AnyVariable',
    'normal variable': 'VariableKind::NormalVariable',
});

const CUSTOM_VALIDATOR_ID = Object.freeze({
    abs: 'CustomValidatorId::Abs',
    dbcreate: 'CustomValidatorId::Dbcreate',
    get_area_param: 'CustomValidatorId::GetAreaParam',
    new_iterator: 'CustomValidatorId::NewIterator',
    pnt_to_plane: 'CustomValidatorId::PntToPlane',
});

function writeGeneratedCpp(catalogs) {
    fs.mkdirSync(GENERATED_DIR, { recursive: true });
    writeGeneratedHeader(catalogs);
    writeCommandCatalogData(catalogs.commands);
    writeKeywordCatalogData(catalogs.keywords);
    writeCommandTypeWritesData(catalogs.commandTypeWrites);
    writeObjectParametersData(catalogs.objectParameters);
}

function writeGeneratedHeader(catalogs) {
    const commandCount = Object.keys(catalogs.commands).length;
    const keywordCount = catalogs.keywords.length;
    const writes = catalogs.commandTypeWrites;
    const body = `${GENERATED_BANNER}#pragma once

#include "catalog/types.h"

#include <cstddef>

namespace vertexadt::catalog::generated {

inline constexpr std::size_t kCommandCount = ${commandCount};
inline constexpr std::size_t kKeywordCount = ${keywordCount};
inline constexpr std::size_t kReturnTypeCount = ${Object.keys(writes.returnTypeMap).length};
inline constexpr std::size_t kInputParamTypeCount = ${Object.keys(writes.inputParamTypeMap).length};
inline constexpr std::size_t kParamTypeCommandCount = ${Object.keys(writes.paramTypeMap).length};
inline constexpr std::size_t kMultiParamTypeCommandCount = ${Object.keys(writes.multiParamTypeMap).length};
inline constexpr std::size_t kObjectParameterCommandCount = ${Object.keys(catalogs.objectParameters).length};

extern const CommandCatalogEntry kCommands[kCommandCount];
extern const KeywordCatalogEntry kKeywords[kKeywordCount];
extern const NameType kReturnTypes[kReturnTypeCount];
extern const NameType kInputParamTypes[kInputParamTypeCount];
extern const CommandSlotTypes kParamTypes[kParamTypeCommandCount];
extern const CommandSlotTypes kMultiParamTypes[kMultiParamTypeCommandCount];
extern const ObjectParameterCommand kObjectParameters[kObjectParameterCommandCount];

} // namespace vertexadt::catalog::generated
`;
    fs.writeFileSync(path.join(GENERATED_DIR, 'catalog_data.h'), body, 'utf8');
}

function writeCommandCatalogData(commands) {
    const names = Object.keys(commands).sort();
    const chunks = [
        GENERATED_BANNER,
        '#include "generated/catalog_data.h"',
        '',
        'namespace vertexadt::catalog::generated {',
        'namespace {',
        '',
    ];

    for (const name of names) {
        chunks.push(emitCommandStatics(name, commands[name]));
    }

    chunks.push('} // namespace');
    chunks.push('');
    chunks.push(`const CommandCatalogEntry kCommands[kCommandCount] = {`);
    for (const name of names) {
        chunks.push(emitCommandEntry(name, commands[name]));
    }
    chunks.push('};');
    chunks.push('');
    chunks.push('} // namespace vertexadt::catalog::generated');
    chunks.push('');
    fs.writeFileSync(path.join(GENERATED_DIR, 'command_catalog_data.cpp'), chunks.join('\n'), 'utf8');
}

function emitCommandStatics(name, row) {
    const lines = [];
    if (row.strictMode) {
        lines.push(...emitStrictMode(name, row.strictMode));
    }
    if (row.cleanupRules) {
        lines.push(...emitCleanupRules(name, row.cleanupRules));
    }
    if (row.tableValidator) {
        lines.push(...emitTableValidator(name, row.tableValidator));
    }
    return lines.join('\n');
}

function emitStrictMode(name, strict) {
    const lines = [];
    const inputSym = emitStrictSlots(lines, name, 'input', strict.inputTypes || []);
    const afterSym = emitStrictSlots(lines, name, 'after', strict.afterParamTypes || []);
    lines.push(`const StrictMode kStrict_${name}{`);
    lines.push(`  .enabled = ${strict.enabled ? 'true' : 'false'},`);
    if (inputSym) {
        lines.push(`  .inputTypes = ${inputSym},`);
    }
    if (afterSym) {
        lines.push(`  .afterParamTypes = ${afterSym},`);
    }
    lines.push('};');
    lines.push('');
    return lines;
}

function emitStrictSlots(lines, name, which, slots) {
    if (slots.length === 0) {
        return null;
    }
    const arrayName = `kStrictSlots_${name}_${which}`;
    for (let i = 0; i < slots.length; i += 1) {
        const slot = slots[i];
        const typesName = `kStrictTypes_${name}_${which}_${i}`;
        lines.push(`const std::string_view ${typesName}[] = {`);
        for (const type of slot.allowedTypes) {
            lines.push(`  ${cppStringView(type)},`);
        }
        lines.push('};');
    }
    lines.push(`const StrictTypeSlot ${arrayName}[] = {`);
    for (let i = 0; i < slots.length; i += 1) {
        const slot = slots[i];
        lines.push('  {');
        lines.push(`    .paramIndex = ${Number(slot.paramIndex)},`);
        lines.push(`    .allowedTypes = kStrictTypes_${name}_${which}_${i},`);
        lines.push(`    .paramName = ${cppStringView(slot.paramName)},`);
        lines.push('  },');
    }
    lines.push('};');
    return arrayName;
}

function emitCleanupRules(name, rules) {
    const lines = [];
    const slots = rules.afterParams || [];
    if (slots.length === 0) {
        lines.push(`const CleanupRules kCleanup_${name}{};`);
        lines.push('');
        return lines;
    }
    lines.push(`const CleanupSlot kCleanupSlots_${name}[] = {`);
    for (const slot of slots) {
        lines.push(`  {.quote = ${quoteRule(slot.quote)}},`);
    }
    lines.push('};');
    lines.push(`const CleanupRules kCleanup_${name}{`);
    lines.push(`  .afterParams = kCleanupSlots_${name},`);
    lines.push('};');
    lines.push('');
    return lines;
}

function emitTableValidator(name, table) {
    const lines = [];
    if (table.assignment) {
        lines.push(`const AssignmentCheck kAssign_${name}{`);
        lines.push(`  .presence = ${assignmentPresence(table.assignment.presence)},`);
        if (table.assignment.kind) {
            lines.push(`  .hasKind = true,`);
            lines.push(`  .kind = ${slotKind(table.assignment.kind)},`);
            lines.push(`  .variableKind = ${variableKind(table.assignment.variableKind)},`);
        }
        if (table.assignment.missing) {
            lines.push(`  .missing = ${cppString(table.assignment.missing)},`);
        }
        if (table.assignment.invalid) {
            lines.push(`  .invalid = ${cppString(table.assignment.invalid)},`);
        }
        lines.push('};');
    }
    if (table.inside && table.inside.length > 0) {
        lines.push(`const Slot kInside_${name}[] = {`);
        for (const slot of table.inside) {
            lines.push(emitSlot(slot));
        }
        lines.push('};');
    }
    if (table.after && table.after.length > 0) {
        lines.push(`const Slot kAfter_${name}[] = {`);
        for (const slot of table.after) {
            lines.push(emitSlot(slot));
        }
        lines.push('};');
    }
    if (table.insideTail) {
        lines.push(`const SlotTail kInsideTail_${name}${emitTail(table.insideTail)};`);
    }
    if (table.afterTail) {
        lines.push(`const SlotTail kAfterTail_${name}${emitTail(table.afterTail)};`);
    }
    lines.push(`const TableValidator kTable_${name}{`);
    if (table.assignment) {
        lines.push(`  .assignment = &kAssign_${name},`);
    }
    if (table.tooFewInside) {
        lines.push(`  .tooFewInside = ${cppString(table.tooFewInside)},`);
    }
    if (table.tooManyInside) {
        lines.push(`  .tooManyInside = ${cppString(table.tooManyInside)},`);
    }
    if (table.tooFewAfter) {
        lines.push(`  .tooFewAfter = ${cppString(table.tooFewAfter)},`);
    }
    if (table.tooManyAfter) {
        lines.push(`  .tooManyAfter = ${cppString(table.tooManyAfter)},`);
    }
    if (table.inside && table.inside.length > 0) {
        lines.push(`  .inside = kInside_${name},`);
    }
    if (table.after && table.after.length > 0) {
        lines.push(`  .after = kAfter_${name},`);
    }
    if (table.insideTail) {
        lines.push(`  .insideTail = &kInsideTail_${name},`);
    }
    if (table.afterTail) {
        lines.push(`  .afterTail = &kAfterTail_${name},`);
    }
    lines.push('};');
    lines.push('');
    return lines;
}

function emitSlot(slot) {
    const fields = [
        `.index = ${Number(slot.index)}`,
        `.kind = ${slotKind(slot.kind)}`,
        `.variableKind = ${variableKind(slot.variableKind)}`,
        `.required = ${slot.required ? 'true' : 'false'}`,
        `.invalid = ${cppStringView(slot.invalid)}`,
    ];
    return `  {${fields.join(', ')}},`;
}

function emitTail(tail) {
    return `{.from = ${Number(tail.from)}, .kind = ${slotKind(tail.kind)}, .variableKind = ${variableKind(tail.variableKind)}, .invalid = ${cppStringView(tail.invalid)}}`;
}

function emitCommandEntry(name, row) {
    const fields = [
        `  .name = ${cppStringView(name)}`,
        `  .structure = ${emitStructure(row.structure)}`,
    ];
    if (row.strictMode) {
        fields.push(`  .strictMode = &kStrict_${name}`);
    }
    if (row.cleanupRules) {
        fields.push(`  .cleanupRules = &kCleanup_${name}`);
    }
    if (row.tableValidator) {
        fields.push(`  .tableValidator = &kTable_${name}`);
    }
    if (row.customValidator) {
        const id = CUSTOM_VALIDATOR_ID[row.customValidator];
        if (!id) {
            throw new Error(`Unknown custom validator id: ${row.customValidator}`);
        }
        fields.push(`  .customValidator = ${id}`);
    }
    return `{\n${fields.join(',\n')},\n},`;
}

function emitStructure(structure) {
    const fields = [
        `.hasAssignment = ${assignmentPresenceFromStructure(structure.hasAssignment)}`,
        `.hasParentheses = ${structure.hasParentheses ? 'true' : 'false'}`,
        `.minParamsInside = ${arity(structure.minParamsInside)}`,
        `.maxParamsInside = ${arity(structure.maxParamsInside)}`,
        `.paramsOutside = ${arity(structure.paramsOutside)}`,
        `.maxParamsOutside = ${arity(structure.maxParamsOutside)}`,
        `.acceptSpacesAsSeparators = ${structure.acceptSpacesAsSeparators ? 'true' : 'false'}`,
        `.valueExpression = ${structure.valueExpression ? 'true' : 'false'}`,
        `.parenthesesOptional = ${structure.parenthesesOptional ? 'true' : 'false'}`,
    ];
    return `{${fields.join(', ')}}`;
}

function writeKeywordCatalogData(keywords) {
    const lines = [
        GENERATED_BANNER,
        '#include "generated/catalog_data.h"',
        '',
        'namespace vertexadt::catalog::generated {',
        '',
        'const KeywordCatalogEntry kKeywords[kKeywordCount] = {',
    ];
    for (const keyword of keywords) {
        const fields = [
            `.label = ${cppStringView(keyword.label)}`,
            `.detail = ${cppStringView(keyword.detail)}`,
        ];
        if (keyword.documentation !== undefined) {
            fields.push(`.documentation = ${cppString(keyword.documentation)}`);
        }
        if (keyword.insertText !== undefined) {
            fields.push(`.insertText = ${cppString(keyword.insertText)}`);
        }
        lines.push(`  {${fields.join(', ')}},`);
    }
    lines.push('};');
    lines.push('');
    lines.push('} // namespace vertexadt::catalog::generated');
    lines.push('');
    fs.writeFileSync(path.join(GENERATED_DIR, 'keyword_catalog_data.cpp'), `${lines.join('\n')}`, 'utf8');
}

function writeCommandTypeWritesData(writes) {
    const paramSlots = emitSlotHelperArrays('Param', writes.paramTypeMap);
    const multiSlots = emitSlotHelperArrays('MultiParam', writes.multiParamTypeMap);
    const lines = [
        GENERATED_BANNER,
        '#include "generated/catalog_data.h"',
        '',
        'namespace vertexadt::catalog::generated {',
        'namespace {',
        '',
        ...paramSlots.helpers,
        ...multiSlots.helpers,
        '} // namespace',
        '',
        ...emitNameTypes('kReturnTypes', 'kReturnTypeCount', writes.returnTypeMap),
        ...emitNameTypes('kInputParamTypes', 'kInputParamTypeCount', writes.inputParamTypeMap),
        ...paramSlots.table,
        ...multiSlots.table,
        '} // namespace vertexadt::catalog::generated',
        '',
    ];
    fs.writeFileSync(path.join(GENERATED_DIR, 'command_type_writes_data.cpp'), lines.join('\n'), 'utf8');
}

function writeObjectParametersData(objectParameters) {
    const helpers = [];
    const table = ['const ObjectParameterCommand kObjectParameters[kObjectParameterCommandCount] = {'];
    for (const command of Object.keys(objectParameters)) {
        const commandIdent = cppIdent(command);
        const listRefs = [];
        for (const objectType of Object.keys(objectParameters[command])) {
            const rows = objectParameters[command][objectType];
            const rowsSymbol = `kRows_${commandIdent}_${cppIdent(objectType)}`;
            if (rows.length > 0) {
                helpers.push(`const ObjectParameterRow ${rowsSymbol}[] = {`);
                for (const row of rows) {
                    helpers.push(
                        `  {${cppStringView(row.label)}, ${optionalView(row.documentation)}, ` +
                        `${optionalView(row.detail)}, ${optionalView(row.insertText)}},`,
                    );
                }
                helpers.push('};');
                helpers.push('');
                listRefs.push(`  {${cppStringView(objectType)}, ${rowsSymbol}},`);
            } else {
                listRefs.push(`  {${cppStringView(objectType)}, {}},`);
            }
        }
        const listsSymbol = `kLists_${commandIdent}`;
        helpers.push(`const ObjectParameterList ${listsSymbol}[] = {`);
        helpers.push(...listRefs);
        helpers.push('};');
        helpers.push('');
        table.push(`  {${cppStringView(command)}, ${listsSymbol}},`);
    }
    table.push('};');
    table.push('');
    const lines = [
        GENERATED_BANNER,
        '#include "generated/catalog_data.h"',
        '',
        'namespace vertexadt::catalog::generated {',
        'namespace {',
        '',
        ...helpers,
        '} // namespace',
        '',
        ...table,
        '} // namespace vertexadt::catalog::generated',
        '',
    ];
    fs.writeFileSync(path.join(GENERATED_DIR, 'object_parameters_data.cpp'), lines.join('\n'), 'utf8');
}

function optionalView(value) {
    return value === undefined ? 'std::nullopt' : cppStringView(value);
}

function emitNameTypes(symbol, countSymbol, map) {
    const lines = [`const NameType ${symbol}[${countSymbol}] = {`];
    for (const name of Object.keys(map).sort()) {
        lines.push(`  {${cppStringView(name)}, ${cppStringView(map[name])}},`);
    }
    lines.push('};');
    lines.push('');
    return lines;
}

function emitSlotHelperArrays(label, map) {
    const names = Object.keys(map).sort();
    const helpers = [];
    for (const name of names) {
        const ident = cppIdent(name);
        const slots = Object.entries(map[name])
            .map(([index, type]) => ({ index: Number(index), type }))
            .sort((a, b) => a.index - b.index);
        helpers.push(`const IndexedType k${label}Slots_${ident}[] = {`);
        for (const slot of slots) {
            helpers.push(`  {${slot.index}, ${cppStringView(slot.type)}},`);
        }
        helpers.push('};');
        helpers.push('');
    }
    const countSymbol = label === 'Param' ? 'kParamTypeCommandCount' : 'kMultiParamTypeCommandCount';
    const arraySymbol = label === 'Param' ? 'kParamTypes' : 'kMultiParamTypes';
    const table = [`const CommandSlotTypes ${arraySymbol}[${countSymbol}] = {`];
    for (const name of names) {
        table.push(`  {${cppStringView(name)}, k${label}Slots_${cppIdent(name)}},`);
    }
    table.push('};');
    table.push('');
    return { helpers, table };
}

function cppIdent(name) {
    return name.replace(/[^A-Za-z0-9_]/g, '_');
}

function cppString(value) {
    return JSON.stringify(value);
}

function cppStringView(value) {
    return `std::string_view{${cppString(value)}}`;
}

function slotKind(kind) {
    const mapped = SLOT_KIND[kind];
    if (!mapped) {
        throw new Error(`Unknown slot kind: ${kind}`);
    }
    return mapped;
}

function variableKind(kind) {
    if (!kind) {
        return 'VariableKind::None';
    }
    const mapped = VARIABLE_KIND[kind];
    if (!mapped) {
        throw new Error(`Unknown variable kind: ${kind}`);
    }
    return mapped;
}

function quoteRule(quote) {
    if (quote === 'never') {
        return 'QuoteRule::Never';
    }
    if (quote === 'bareNumericLiteral') {
        return 'QuoteRule::BareNumericLiteral';
    }
    throw new Error(`Unknown quote rule: ${quote}`);
}

function assignmentPresence(presence) {
    if (presence === 'required') {
        return 'AssignmentPresence::Required';
    }
    if (presence === 'optional') {
        return 'AssignmentPresence::Optional';
    }
    if (presence === 'forbidden') {
        return 'AssignmentPresence::Forbidden';
    }
    throw new Error(`Unknown assignment presence: ${presence}`);
}

function assignmentPresenceFromStructure(value) {
    if (value === true) {
        return 'AssignmentPresence::Required';
    }
    if (value === false) {
        return 'AssignmentPresence::Forbidden';
    }
    if (value === 'optional') {
        return 'AssignmentPresence::Optional';
    }
    throw new Error(`Unknown structure.hasAssignment: ${value}`);
}

function arity(value) {
    if (value === undefined) {
        return 'Arity::omitted()';
    }
    if (value === 'unbounded') {
        return 'Arity::unbounded()';
    }
    if (typeof value === 'number') {
        return `Arity::finite(${value})`;
    }
    throw new Error(`Unknown arity: ${value}`);
}

if (require.main === module) {
    main();
}
