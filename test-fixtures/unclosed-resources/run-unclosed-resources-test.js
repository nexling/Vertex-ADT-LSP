'use strict';

const path = require('path');
const unclosedResources = require('../../server/out/unclosedResources');

function normalizePath(filePath) {
    return path.normalize(filePath).replace(/\\/g, '/').toLowerCase();
}

function makeContext(files, current, includeLinks) {
    const fileMap = new Map();
    Object.keys(files).forEach((name) => {
        fileMap.set(normalizePath(name), String(files[name]).split(/\r?\n/));
    });
    const parentMap = new Map();
    const outgoingMap = new Map();
    (includeLinks || []).forEach((link) => {
        const from = normalizePath(link.from);
        const to = normalizePath(link.to);
        if (!parentMap.has(to)) {
            parentMap.set(to, []);
        }
        parentMap.get(to).push({
            fromPath: from,
            line: link.line || 0,
            lineText: link.lineText || `# include ${link.to}`,
            targetPath: to
        });
        if (!outgoingMap.has(from)) {
            outgoingMap.set(from, []);
        }
        outgoingMap.get(from).push({
            targetPath: to,
            line: link.line || 0,
            lineText: link.lineText || `# include ${link.to}`
        });
    });
    return {
        currentFilePath: normalizePath(current),
        getFileLines: (filePath) => fileMap.get(normalizePath(filePath)) || null,
        resolvePath: (rawPath) => {
            const stripped = String(rawPath || '').replace(/['"]/g, '').replace(/\\/g, '/');
            const full = normalizePath(stripped);
            if (fileMap.has(full)) {
                return full;
            }
            const base = normalizePath(path.posix.basename(stripped));
            if (fileMap.has(base)) {
                return base;
            }
            return null;
        },
        normalizePath,
        getDirectIncludeParents: (target) => parentMap.get(normalizePath(target)) || [],
        getOutgoingIncludeLinks: (from) => outgoingMap.get(normalizePath(from)) || [],
        filePathToUri: (filePath) => `file:///${normalizePath(filePath)}`
    };
}

function issuesFor(files, current, includeLinks) {
    return unclosedResources.collectUnclosedResourceIssues(makeContext(files, current, includeLinks));
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertCount(issues, expected, label) {
    assert(issues.length === expected, `${label}: expected ${expected} issue(s), got ${issues.length} (${issues.map((i) => i.message).join(' | ')})`);
}

function assertHas(issues, snippet, label) {
    const found = issues.some((issue) => issue.message.includes(snippet));
    assert(found, `${label}: expected a message containing "${snippet}", got: ${issues.map((i) => i.message).join(' | ')}`);
}

function assertSeverity(issues, snippet, severity, label) {
    const issue = issues.find((item) => item.message.includes(snippet));
    assert(issue, `${label}: missing "${snippet}"`);
    assert(issue.severity === severity, `${label}: expected ${severity} for "${snippet}", got ${issue.severity}`);
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "SHEETLIB",1\n'
    }, 'main.prg');
    assertCount(issues, 1, 'unclosed dbopen');
    assertHas(issues, 'Database 1 is opened here', 'unclosed dbopen');
    assertSeverity(issues, 'Database 1', 'warning', 'unclosed dbopen');
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "SHEETLIB",1\ndbclose(1)\n'
    }, 'main.prg');
    assertCount(issues, 0, 'matched dbopen/dbclose');
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "SHEETLIB",1\ndbclose(1)\ndbopen(1) "SHEETLIB",1\n'
    }, 'main.prg');
    assertCount(issues, 1, 'open after close');
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "A",1\ndbopen(1) "A",1\ndbclose(1)\n'
    }, 'main.prg');
    assertCount(issues, 0, 'two opens then one later close');
}

{
    const issues = issuesFor({
        'main.prg': 'open(1) "file.txt"\ndbclose(1)\n'
    }, 'main.prg');
    assertCount(issues, 1, 'file open not closed by dbclose');
    assertHas(issues, 'File 1 is opened here', 'file vs db namespace');
}

{
    const issues = issuesFor({
        'main.prg': 'open(1) "file.txt"\nclose(1)\n'
    }, 'main.prg');
    assertCount(issues, 0, 'matched open/close');
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "ALREADY_OPEN",OtherId\n'
    }, 'main.prg');
    assertCount(issues, 1, 'ALREADY_OPEN');
    assertSeverity(issues, 'ALREADY_OPEN', 'information', 'ALREADY_OPEN');
    assert(issues[0].code === unclosedResources.UNCLOSED_ALREADY_OPEN_CODE, 'ALREADY_OPEN code');
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "SHEETLIB",1 ;!IGNORE CLOSE\n'
    }, 'main.prg');
    assertCount(issues, 0, 'IGNORE CLOSE on dbopen');
}

{
    const issues = issuesFor({
        'main.prg': [
            'common_dbases 1',
            'dbopen(1) "SHEETLIB",1',
            'call("closer.prg")'
        ].join('\n'),
        'closer.prg': 'dbclose(1)\n'
    }, 'main.prg');
    assertCount(issues, 0, 'callee close with common_dbases');
}

{
    const issues = issuesFor({
        'main.prg': [
            'dbopen(1) "SHEETLIB",1',
            'call("closer.prg")'
        ].join('\n'),
        'closer.prg': 'dbclose(1)\n'
    }, 'main.prg');
    assertCount(issues, 1, 'callee close without common_dbases');
}

{
    const issues = issuesFor({
        'main.prg': '# include opener.prg\ndbclose(1)\n',
        'opener.prg': 'dbopen(1) "SHEETLIB",1\n'
    }, 'main.prg');
    assertCount(issues, 0, 'include then close in parent');
}

{
    const issues = issuesFor({
        'main.prg': '# include opener.prg\n',
        'opener.prg': 'dbopen(1) "SHEETLIB",1\n'
    }, 'main.prg');
    assertCount(issues, 1, 'include leftover on include line');
    assertHas(issues, 'Included code opens database 1', 'include leftover');
}

{
    const issues = issuesFor({
        'main.prg': '# include opener.prg ;!IGNORE CLOSE\n',
        'opener.prg': 'dbopen(1) "SHEETLIB",1\n'
    }, 'main.prg');
    assertCount(issues, 0, 'IGNORE CLOSE on include');
}

{
    const issues = issuesFor({
        'main.prg': 'dbopen(1) "C:/lib/PROREG",1\nok = dbclose_by_name(\'PROREG\')\n'
    }, 'main.prg');
    assertCount(issues, 0, 'dbclose_by_name basename');
}

{
    const issues = issuesFor({
        'main.prg': '# include sectional.prg USED\n',
        'sectional.prg': [
            '#USED#',
            'n = 1',
            '#USED#',
            '#OTHER#',
            'dbopen(1) "SHEETLIB",1',
            '#OTHER#'
        ].join('\n')
    }, 'main.prg');
    assertCount(issues, 0, 'section include ignores other section open');
}

{
    const opener = 'dbopen(1) "SHEETLIB",1\n';
    const issues = issuesFor({
        'fragment.prg': opener,
        'good_root.prg': '# include fragment.prg\ndbclose(1)\n',
        'bad_root.prg': '# include fragment.prg\n'
    }, 'fragment.prg', [
        { from: 'good_root.prg', to: 'fragment.prg', line: 0 },
        { from: 'bad_root.prg', to: 'fragment.prg', line: 0 }
    ]);
    assertCount(issues, 1, 'include-upward requires every include-root');
    assert(issues[0].related && issues[0].related.length === 1, 'relatedInformation only for failing root');
}

{
    const issues = issuesFor({
        'fragment.prg': 'dbopen(1) "SHEETLIB",1\n',
        'good_root.prg': '# include fragment.prg\ndbclose(1)\n'
    }, 'fragment.prg', [
        { from: 'good_root.prg', to: 'fragment.prg', line: 0 }
    ]);
    assertCount(issues, 0, 'single include-root that closes silences fragment');
}

{
    const issues = issuesFor({
        'main.prg': 'dbcreate(2) "TEMPDB",0,0\n'
    }, 'main.prg');
    assertCount(issues, 1, 'dbcreate is an open');
}

{
    const insertion = unclosedResources.buildIgnoreCloseInsertion('dbopen(1) "A",1');
    assert(insertion && insertion.newText === ' ;!IGNORE CLOSE', 'quick-fix insert comment');
    const append = unclosedResources.buildIgnoreCloseInsertion('dbopen(1) "A",1 ;!keep');
    assert(append && append.newText === ' IGNORE CLOSE', 'quick-fix append to existing ;!');
    assert(unclosedResources.buildIgnoreCloseInsertion('dbopen(1) "A",1 ;!IGNORE CLOSE') === null, 'quick-fix skip existing');
}

console.log('unclosed-resources: all assertions passed');
