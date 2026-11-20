'use strict';

const fs = require('fs');
const path = require('path');

const fixtureRoot = __dirname;

function parseFileSections(lines) {
    const sectionMarker = /^#([A-Za-z_][A-Za-z0-9_]*)#$/;
    const open = new Map();
    const sections = [];

    for (let i = 0; i < lines.length; i++) {
        if (!lines[i].startsWith('#')) {
            continue;
        }
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

function isDebugDocumentText(text) {
    return text.includes('START OF DEBUG in FILE') || text.includes('START OF DEBUG in SECTION');
}

function isVxdbaseWinstyleShellDocumentText(text) {
    if (!text) {
        return false;
    }
    const lines = text.split(/\r?\n/);
    if (parseFileSections(lines).length > 0) {
        return false;
    }
    for (let i = 0; i < lines.length; i++) {
        const trimmed = lines[i].trim();
        if (!trimmed || trimmed.startsWith('!')) {
            continue;
        }
        if (/^vxdbase_winstyle\b/i.test(trimmed)) {
            return true;
        }
    }
    return false;
}

function isValidationExcludedDocumentText(text) {
    return isDebugDocumentText(text) || isVxdbaseWinstyleShellDocumentText(text);
}

function readFixture(name) {
    return fs.readFileSync(path.join(fixtureRoot, name), 'utf8');
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

assert(
    isVxdbaseWinstyleShellDocumentText(readFixture('shell_no_sections.prg')),
    'shell without sections and with vxdbase_winstyle should be excluded'
);
assert(
    isValidationExcludedDocumentText(readFixture('shell_no_sections.prg')),
    'shell file should be validation-excluded'
);

assert(
    !isVxdbaseWinstyleShellDocumentText(readFixture('with_sections.prg')),
    'file with sections should not match shell rule'
);
assert(
    !isValidationExcludedDocumentText(readFixture('with_sections.prg')),
    'sectioned file with vxdbase_winstyle should still be validated'
);

assert(
    !isVxdbaseWinstyleShellDocumentText(readFixture('normal_no_marker.prg')),
    'normal file without sections or winstyle should not match shell rule'
);
assert(
    !isValidationExcludedDocumentText(readFixture('normal_no_marker.prg')),
    'normal file should still be validated'
);

assert(
    !isVxdbaseWinstyleShellDocumentText('! vxdbase_winstyle(1)'),
    'winstyle line in comment should not match'
);

console.log('vxdbase-winstyle-shell: all assertions passed');
