'use strict';

const fs = require('fs');
const path = require('path');

const CALLEE_COMMON_REQUIREMENTS = [
    { marker: '@require_common_indexes', directive: 'common_indexes', callerPattern: /^common_indexes\s+1\b/i },
    { marker: '@require_common_dbases', directive: 'common_dbases', callerPattern: /^common_dbases\s+1\b/i },
    { marker: '@require_common_elements', directive: 'common_elements', callerPattern: /^common_elements\s+1\b/i },
    { marker: '@require_common_files', directive: 'common_files', callerPattern: /^common_files\s+1\b/i },
    { marker: '@require_common_points', directive: 'common_points', callerPattern: /^common_points\s+1\b/i }
];

function parseFileSections(lines) {
    const sectionMarker = /^#([A-Za-z_][A-Za-z0-9_]*)#$/;
    const open = new Map();
    const sections = [];

    for (let i = 0; i < lines.length; i++) {
        if (!(lines[i].startsWith('#'))) {continue;}
        const m = lines[i].trim().match(sectionMarker);
        if (!m) {
            continue;
        }

        const name = m[1];
        if (!open.has(name)) {
            open.set(name, i);
        } else {
            const start = open.get(name);
            open.delete(name);
            sections.push({
                name,
                markerStart: start,
                markerEnd: i,
                bodyStart: start + 1,
                bodyEnd: i - 1,
            });
        }
    }
    return sections;
}

function getSectionForLine(lineIndex, fileSections) {
    for (let i = 0; i < fileSections.length; i++) {
        const section = fileSections[i];
        if (lineIndex >= section.bodyStart && lineIndex <= section.bodyEnd) {
            return section.name;
        }
    }
    return '';
}

function parseCalleeCommonRequirements(lines) {
    const required = [];
    const seen = new Set();
    const fileSections = parseFileSections(lines);
    for (let i = 0; i < lines.length; i++) {
        const trimmed = lines[i].trim();
        if (trimmed.startsWith('!')) {
            continue;
        }
        const sectionName = getSectionForLine(i, fileSections) || '';
        for (const spec of CALLEE_COMMON_REQUIREMENTS) {
            if (trimmed.startsWith(spec.marker)) {
                const key = `${spec.directive}::${sectionName}`;
                if (!seen.has(key)) {
                    seen.add(key);
                    required.push({
                        directive: spec.directive,
                        sectionName
                    });
                }
            }
        }
    }
    return required;
}

function selectCalleeCommonRequirements(requirements, targetSection) {
    const selected = new Set();
    const section = targetSection || '';
    for (const req of requirements) {
        const directive = typeof req === 'string' ? req : req.directive;
        const reqSection = typeof req === 'string' ? '' : (req.sectionName || '');
        if (!section) {
            selected.add(directive);
            continue;
        }
        if (reqSection === '' || reqSection === section) {
            selected.add(directive);
        }
    }
    return [...selected];
}

function callerHasCommonDirective(lines, spec, sourceSection) {
    const fileSections = parseFileSections(lines);
    const wantedSection = sourceSection || '';
    for (let i = 0; i < lines.length; i++) {
        const lineSection = getSectionForLine(i, fileSections) || '';
        if (lineSection !== wantedSection) {
            continue;
        }
        const trimmed = lines[i].trim();
        if (trimmed.startsWith('!')) {
            continue;
        }
        if (spec.callerPattern.test(trimmed)) {
            return true;
        }
    }
    return false;
}

function findMissingCalleeRequirements(callerLines, calleeLines, sourceSection, targetSection) {
    const missing = [];
    const requiredDirectives = selectCalleeCommonRequirements(
        parseCalleeCommonRequirements(calleeLines),
        targetSection
    );
    for (const directiveName of requiredDirectives) {
        const spec = CALLEE_COMMON_REQUIREMENTS.find((entry) => entry.directive === directiveName);
        if (spec && !callerHasCommonDirective(callerLines, spec, sourceSection)) {
            missing.push(spec);
        }
    }
    return missing;
}

function requirementNames(entries) {
    return [...new Set(entries.map((entry) => entry.directive))];
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

assert(
    requirementNames(parseCalleeCommonRequirements(['@require_common_indexes'])).includes('common_indexes'),
    'callee marker should parse common_indexes requirement'
);

assert(
    requirementNames(parseCalleeCommonRequirements(['@require_common_dbases', '@require_common_points'])).length === 2,
    'multiple callee markers should parse'
);

assert(
    callerHasCommonDirective(['common_indexes 1'], CALLEE_COMMON_REQUIREMENTS[0]),
    'common_indexes 1 should satisfy indexes requirement'
);

assert(
    !callerHasCommonDirective(['common_indexes 0'], CALLEE_COMMON_REQUIREMENTS[0]),
    'common_indexes 0 should not satisfy indexes requirement'
);

const callerMissingPath = path.join(__dirname, 'custom', 'caller_missing_common.prg');
const calleeWithCommonPath = path.join(__dirname, 'custom', 'callee_with_common.prg');
const callerNoCommonPath = path.join(__dirname, 'custom', 'caller_no_common.prg');
const calleeNoCommonPath = path.join(__dirname, 'custom', 'callee_no_common.prg');

const callerMissingLines = fs.readFileSync(callerMissingPath, 'utf8').split(/\r?\n/);
const calleeWithCommonLines = fs.readFileSync(calleeWithCommonPath, 'utf8').split(/\r?\n/);
const callerNoCommonLines = fs.readFileSync(callerNoCommonPath, 'utf8').split(/\r?\n/);
const calleeNoCommonLines = fs.readFileSync(calleeNoCommonPath, 'utf8').split(/\r?\n/);

const missingForCalleeWithCommon = findMissingCalleeRequirements(callerMissingLines, calleeWithCommonLines);
assert(
    missingForCalleeWithCommon.some((spec) => spec.directive === 'common_indexes'),
    'caller without common_indexes 1 should miss requirement for callee_with_common'
);

const missingForCalleeNoCommon = findMissingCalleeRequirements(callerNoCommonLines, calleeNoCommonLines);
assert(
    missingForCalleeNoCommon.length === 0,
    'callee without @require_common_* should not impose caller requirements'
);

const callerWithIndexes = ['common_indexes 1', 'ok = call("foo.prg")'];
assert(
    findMissingCalleeRequirements(callerWithIndexes, calleeWithCommonLines).length === 0,
    'caller with common_indexes 1 should satisfy callee requirement'
);

const includeOnlyCaller = ['# include "other.prg"'];
assert(
    parseCalleeCommonRequirements(includeOnlyCaller).length === 0,
    'non-callee files should not declare requirements accidentally'
);

const sectionedCallee = [
    '#JOINT_TEST#',
    '@require_common_indexes',
    '#JOINT_TEST#',
    '#JOINT_DETAILING#',
    '@require_common_points',
    '#JOINT_DETAILING#'
];
const sectionedCalleeReqs = parseCalleeCommonRequirements(sectionedCallee);
const wholeFileReqs = selectCalleeCommonRequirements(sectionedCalleeReqs, '');
const jointTestTargetReqs = selectCalleeCommonRequirements(sectionedCalleeReqs, 'JOINT_TEST');
const jointDetailTargetReqs = selectCalleeCommonRequirements(sectionedCalleeReqs, 'JOINT_DETAILING');
const unknownTargetReqs = selectCalleeCommonRequirements(sectionedCalleeReqs, 'JOINT_MISSING');

assert(
    wholeFileReqs.includes('common_indexes') && wholeFileReqs.includes('common_points'),
    'no-suffix call should use every callee marker, not inherit the caller section'
);
assert(
    jointTestTargetReqs.includes('common_indexes') && !jointTestTargetReqs.includes('common_points'),
    'explicit JOINT_TEST target should use JOINT_TEST markers only, not JOINT_DETAILING'
);
assert(
    jointDetailTargetReqs.includes('common_points') && !jointDetailTargetReqs.includes('common_indexes'),
    'explicit JOINT_DETAILING target should use JOINT_DETAILING markers only, not JOINT_TEST'
);
assert(
    unknownTargetReqs.length === 0,
    'unknown explicit target should use file-level markers only'
);

const fileLevelCallee = ['@require_common_indexes', '#JOINT_TEST#', '#JOINT_TEST#'];
assert(
    selectCalleeCommonRequirements(parseCalleeCommonRequirements(fileLevelCallee), '').includes('common_indexes'),
    'file-level callee marker applies to a no-suffix whole-file call'
);
assert(
    selectCalleeCommonRequirements(parseCalleeCommonRequirements(fileLevelCallee), 'JOINT_TEST').includes('common_indexes'),
    'file-level callee marker still applies when targeting a section'
);
assert(
    selectCalleeCommonRequirements(parseCalleeCommonRequirements(fileLevelCallee), 'JOINT_MISSING').includes('common_indexes'),
    'unknown target still applies file-level callee markers'
);

const calleeWithInclude = ['# include "extra.prg"', '@require_common_files'];
assert(
    requirementNames(parseCalleeCommonRequirements(calleeWithInclude)).join(',') === 'common_files',
    'callee # include does not contribute markers from the included file'
);

const jointCallerMissingInSection = [
    'common_indexes 1',
    '#JOINT_TEST#',
    'ok = call("foo.prg")',
    '#JOINT_TEST#',
    '#JOINT_DETAILING#',
    'common_indexes 1',
    'ok = call("foo.prg")',
    '#JOINT_DETAILING#'
];
assert(
    findMissingCalleeRequirements(jointCallerMissingInSection, fileLevelCallee, 'JOINT_TEST').length === 1,
    'file-level common_indexes 1 should not satisfy a JOINT_TEST call'
);
assert(
    findMissingCalleeRequirements(jointCallerMissingInSection, fileLevelCallee, 'JOINT_DETAILING').length === 0,
    'common_indexes 1 inside JOINT_DETAILING should satisfy a JOINT_DETAILING call'
);
assert(
    findMissingCalleeRequirements(jointCallerMissingInSection, fileLevelCallee, '').length === 0,
    'file-level common_indexes 1 should satisfy a file-level call'
);

const fileLevelCallerWithSectionDirective = [
    '#JOINT_TEST#',
    'common_indexes 1',
    '#JOINT_TEST#',
    'ok = call("foo.prg")'
];
assert(
    findMissingCalleeRequirements(fileLevelCallerWithSectionDirective, fileLevelCallee, '').length === 1,
    'common_indexes 1 inside JOINT_TEST should not satisfy a file-level call'
);

const jointCallerWithSectionDirective = [
    '#JOINT_TEST#',
    'common_indexes 1',
    'ok = call("foo.prg")',
    '#JOINT_TEST#',
    '#JOINT_DETAILING#',
    'ok = call("foo.prg")',
    '#JOINT_DETAILING#'
];
const jointTestWholeFileMissing = findMissingCalleeRequirements(
    jointCallerWithSectionDirective,
    sectionedCallee,
    'JOINT_TEST'
);
assert(
    jointTestWholeFileMissing.length === 1 && jointTestWholeFileMissing[0].directive === 'common_points',
    'no-suffix JOINT_TEST call should miss JOINT_DETAILING points requirement from the whole callee'
);
const jointDetailWholeFileMissing = findMissingCalleeRequirements(
    jointCallerWithSectionDirective,
    sectionedCallee,
    'JOINT_DETAILING'
);
assert(
    jointDetailWholeFileMissing.length === 2,
    'no-suffix JOINT_DETAILING call should miss both whole-file callee requirements'
);
assert(
    findMissingCalleeRequirements(jointCallerWithSectionDirective, sectionedCallee, 'JOINT_TEST', 'JOINT_TEST').length === 0,
    'explicit JOINT_TEST target is satisfied by common_indexes 1 in JOINT_TEST'
);
assert(
    findMissingCalleeRequirements(jointCallerWithSectionDirective, sectionedCallee, 'JOINT_DETAILING', 'JOINT_TEST')[0].directive === 'common_indexes',
    'explicit JOINT_TEST target from JOINT_DETAILING still requires common_indexes 1 in JOINT_DETAILING'
);

console.log('callee require_common smoke test passed');
