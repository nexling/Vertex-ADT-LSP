'use strict';

function editMayAffectCrossFileDiagnostics(result) {
    const linesToCheck = [result.newLine, result.added, result.removed].filter(Boolean);
    const crossFilePatterns = [
        /\bcall\s*\(/i,
        /\bcommon_(indexes|dbases|points)\b/i,
        /#include\b/i,
        /^##included_from/i,
        /@declarationnumbercheck/i,
        /\b[A-Za-z][A-Za-z0-9_-]*\s*=\s*\d+/,
    ];

    for (let i = 0; i < linesToCheck.length; i++) {
        const line = linesToCheck[i];
        for (let j = 0; j < crossFilePatterns.length; j++) {
            if (crossFilePatterns[j].test(line)) {
                return true;
            }
        }
    }

    return result.removed && result.removed.includes('\n');
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

assert(
    !editMayAffectCrossFileDiagnostics({ newLine: 'x = y + 1', added: '+ 1', removed: '' }),
    'ordinary expression edits should not trigger connected refresh'
);
assert(
    editMayAffectCrossFileDiagnostics({ newLine: 'call(otherModule)', added: 'call', removed: '' }),
    'call edits should trigger connected refresh when other files are open in scope'
);
assert(
    editMayAffectCrossFileDiagnostics({ newLine: 'extravar = 105', added: '5', removed: '4' }),
    'numeric declaration edits should trigger connected refresh'
);
assert(
    editMayAffectCrossFileDiagnostics({ newLine: '', added: '', removed: '\n' }),
    'structural line removals should trigger connected refresh'
);

console.log('diagnostic refresh heuristic test passed');
