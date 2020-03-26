"use strict";
/**
 * @license
 * Copyright (c) 2016 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt
 * The complete set of authors may be found at
 * http://polymer.github.io/AUTHORS.txt
 * The complete set of contributors may be found at
 * http://polymer.github.io/CONTRIBUTORS.txt
 * Code distributed by Google as part of the polymer project is also
 * subject to an additional IP rights grant found at
 * http://polymer.github.io/PATENTS.txt
 */
Object.defineProperty(exports, "__esModule", { value: true });
const espree = require("espree");
const model_1 = require("../model/model");
const javascript_document_1 = require("./javascript-document");
// TODO(rictic): stop exporting this.
exports.baseParseOptions = {
    ecmaVersion: 8,
    attachComment: true,
    comment: true,
    loc: true,
};
class JavaScriptParser {
    parse(contents, url, inlineInfo) {
        const isInline = !!inlineInfo;
        inlineInfo = inlineInfo || {};
        const result = parseJs(contents, url, inlineInfo.locationOffset, undefined, this.sourceType);
        if (result.type === 'failure') {
            // TODO(rictic): define and return a ParseResult instead of throwing.
            const minimalDocument = new javascript_document_1.JavaScriptDocument({
                url,
                contents,
                ast: null,
                locationOffset: inlineInfo.locationOffset,
                astNode: inlineInfo.astNode, isInline,
                parsedAsSourceType: 'script',
            });
            throw new model_1.WarningCarryingException(new model_1.Warning(Object.assign({ parsedDocument: minimalDocument }, result.warning)));
        }
        return new javascript_document_1.JavaScriptDocument({
            url,
            contents,
            ast: result.program,
            locationOffset: inlineInfo.locationOffset,
            astNode: inlineInfo.astNode, isInline,
            parsedAsSourceType: result.sourceType,
        });
    }
}
exports.JavaScriptParser = JavaScriptParser;
class JavaScriptModuleParser extends JavaScriptParser {
    constructor() {
        super(...arguments);
        this.sourceType = 'module';
    }
}
exports.JavaScriptModuleParser = JavaScriptModuleParser;
class JavaScriptScriptParser extends JavaScriptParser {
    constructor() {
        super(...arguments);
        this.sourceType = 'script';
    }
}
exports.JavaScriptScriptParser = JavaScriptScriptParser;
/**
 * Parse the given contents and return either an AST or a parse error as a
 * Warning.
 *
 * It needs the filename and the location offset to produce correct warnings.
 */
function parseJs(contents, file, locationOffset, warningCode, sourceType) {
    if (!warningCode) {
        warningCode = 'parse-error';
    }
    let program;
    try {
        // If sourceType is not provided, we will try script first and if that
        // fails, we will try module, since failure is probably that it can't parse
        // the 'import' or 'export' syntax as a script.
        if (!sourceType) {
            try {
                sourceType = 'script';
                program = espree.parse(contents, Object.assign({ sourceType }, exports.baseParseOptions));
            }
            catch (_ignored) {
                sourceType = 'module';
                program = espree.parse(contents, Object.assign({ sourceType }, exports.baseParseOptions));
            }
        }
        else {
            program = espree.parse(contents, Object.assign({ sourceType }, exports.baseParseOptions));
        }
        return {
            type: 'success',
            sourceType: sourceType,
            program: program,
        };
    }
    catch (err) {
        if (err instanceof SyntaxError) {
            return {
                type: 'failure',
                warning: {
                    message: err.message.split('\n')[0],
                    severity: model_1.Severity.ERROR,
                    code: warningCode,
                    sourceRange: model_1.correctSourceRange({
                        file,
                        start: { line: err.lineNumber - 1, column: err.column - 1 },
                        end: { line: err.lineNumber - 1, column: err.column - 1 }
                    }, locationOffset)
                }
            };
        }
        throw err;
    }
}
exports.parseJs = parseJs;

//# sourceMappingURL=javascript-parser.js.map
