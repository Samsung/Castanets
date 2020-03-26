/**
@license
Copyright (c) 2016 The Polymer Project Authors. All rights reserved.
This code may only be used under the BSD style license found at http://polymer.github.io/LICENSE.txt
The complete set of authors may be found at http://polymer.github.io/AUTHORS.txt
The complete set of contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt
Code distributed by Google as part of the polymer project is also
subject to an additional IP rights grant found at http://polymer.github.io/PATENTS.txt
*/

'use strict';
const dom5 = require('dom5');

const pathResolver = require('./lib/pathresolver.js');

let ApplyShim, CssParse, StyleTransformer, StyleUtil;
const loadShadyCSS = require('./lib/shadycss-entrypoint.js');

const path = require('path');

/** @type {number} */
const AnalyzerVersion = require(path.resolve(require.resolve('polymer-analyzer'), '../../package.json')).version.match(/\d+/)[0];

const {Analyzer, InMemoryOverlayUrlLoader} = require('polymer-analyzer');

// Use `Analysis` from polymer-analyzer for typing
/* eslint-disable no-unused-vars */
const {Analysis} = require('polymer-analyzer/lib/model/model.js');
/* eslint-enable */

const {dirShadowTransform, slottedToContent} = require('./lib/polymer-1-transforms.js');

const pred = dom5.predicates;

const domModuleCache = Object.create(null);

// TODO: upstream to dom5
const styleMatch = pred.AND(
  pred.hasTagName('style'),
  pred.OR(
    pred.NOT(
      pred.hasAttr('type')
    ),
    pred.hasAttrValue('type', 'text/css')
  )
);

const notStyleMatch = pred.NOT(styleMatch);

const customStyleMatchV1 = pred.AND(
  styleMatch,
  pred.hasAttrValue('is', 'custom-style')
);

const customStyleMatchV2 = pred.AND(
  styleMatch,
  pred.parentMatches(
    pred.hasTagName('custom-style')
  )
);

const styleIncludeMatch = pred.AND(styleMatch, pred.hasAttr('include'));

const scopeMap = new WeakMap();

function prepend(parent, node) {
  if (parent.childNodes.length > 0) {
    dom5.insertBefore(parent, parent.childNodes[0], node);
  } else {
    dom5.append(parent, node);
  }
}

/*
 * Collect styles from dom-module
 * In addition, make sure those styles are inside a template
 */
function getAndFixDomModuleStyles(domModule) {
  // TODO: support `.styleModules = ['module-id', ...]` ?
  const styles = dom5.queryAll(domModule, styleMatch, undefined, dom5.childNodesIncludeTemplate);
  if (!styles.length) {
    return [];
  }
  let template = dom5.query(domModule, pred.hasTagName('template'));
  if (!template) {
    template = dom5.constructors.element('template');
    const content = dom5.constructors.fragment();
    styles.forEach(s => dom5.append(content, s));
    dom5.append(template, content);
    dom5.append(domModule, template);
  } else {
    styles.forEach((s) => {
      let templateContent = template.content;
      // ensure element styles are inside the template element
      const parent = dom5.nodeWalkAncestors(s, (n) =>
        n === templateContent || n === domModule
      );
      if (parent !== templateContent) {
        prepend(templateContent, s);
      }
    })
  }
  return styles;
}

// TODO: consider upstreaming to dom5
function getAttributeArray(node, attribute) {
  const attr = dom5.getAttribute(node, attribute);
  let array;
  if (!attr) {
    array = [];
  } else {
    array = attr.split(' ');
  }
  return array;
}

function inlineStyleIncludes(style) {
  if (!styleIncludeMatch(style)) {
    return;
  }
  const styleText = [];
  const includes = getAttributeArray(style, 'include');
  const leftover = [];
  const baseDocument = style.__ownerDocument;
  includes.forEach((id) => {
    const domModule = domModuleCache[id];
    if (!domModule) {
      // we missed this one, put it back on later
      leftover.push(id);
      return;
    }
    const includedStyles = getAndFixDomModuleStyles(domModule);
    // gather included styles
    includedStyles.forEach((ism) => {
      // this style may also have includes
      inlineStyleIncludes(ism);
      const inlineDocument = domModule.__ownerDocument;
      let includeText = dom5.getTextContent(ism);
      // adjust paths
      includeText = pathResolver.rewriteURL(inlineDocument, baseDocument, includeText);
      styleText.push(includeText);
    });
  });
  // remove inlined includes
  if (leftover.length) {
    dom5.setAttribute(style, 'include', leftover.join(' '));
  } else {
    dom5.removeAttribute(style, 'include');
  }
  // prepend included styles
  if (styleText.length) {
    let text = dom5.getTextContent(style);
    text = styleText.join('') + text;
    dom5.setTextContent(style, text);
  }
}

function applyShim(ast) {
  /*
   * `transform` expects an array of decorated <style> elements
   *
   * Decorated <style> elements are ones with `__cssRules` property
   * with a value of the CSS ast
   */
  StyleUtil.forEachRule(ast, (rule) => ApplyShim.transformRule(rule));
}

function getModuleDefinition(moduleName, elementDescriptors) {
  for (let ed of elementDescriptors) {
    if (ed.tagName && ed.tagName.toLowerCase() === moduleName) {
      return ed;
    }
  }
  return null;
}

function shadyShim(ast, style, analysis) {
  const scope = scopeMap.get(style);
  const moduleDefinition = getModuleDefinition(scope, analysis.getFeatures({kind: 'element'}));
  // only shim if module is a full polymer element, not just a style module
  if (!scope || !moduleDefinition) {
    return;
  }
  const ext = moduleDefinition.extends;
  StyleTransformer.css(ast, scope, ext);
}

function addClass(node, className) {
  const classList = getAttributeArray(node, 'class');
  if (classList.indexOf('style-scope') === -1) {
    classList.push('style-scope');
  }
  if (classList.indexOf(className) === -1) {
    classList.push(className);
  }
  dom5.setAttribute(node, 'class', classList.join(' '));
}

function markElement(domModule, scope, useNativeShadow, markTemplate = true) {
  const buildType = useNativeShadow ? 'shadow' : 'shady';
  // apply scoping to dom-module
  dom5.setAttribute(domModule, 'css-build', buildType);
  // apply scoping to template
  const template = dom5.query(domModule, pred.hasTagName('template'));
  if (template) {
    if (markTemplate) {
      dom5.setAttribute(template, 'css-build', buildType);
    }
    // mark elements' subtree under shady build
    if (buildType === 'shady' && scope) {
      const elements = dom5.queryAll(template, notStyleMatch, undefined, dom5.childNodesIncludeTemplate);
      elements.forEach((el) => addClass(el, scope));
    }
  }
}

// For forward compatibility with ShadowDOM v1 and Polymer 2.x,
// replace ::slotted(${inner}) with ::content > ${inner}
function slottedTransform(ast) {
  StyleUtil.forEachRule(ast, (rule) => {
    rule.selector = slottedToContent(rule.selector);
  });
}

function dirTransform(ast) {
  StyleUtil.forEachRule(ast, (rule) => {
    rule.selector = dirShadowTransform(rule.selector);
  });
}

function setUpLibraries(useNativeShadow) {
  ({
    ApplyShim,
    CssParse,
    StyleTransformer,
    StyleUtil
  } = loadShadyCSS(useNativeShadow));
}

function setNodeFileLocation(node, analysisKind) {
  if (!node.__ownerDocument) {
    node.__ownerDocument = analysisKind.sourceRange.file;
  }
}

/**
 *
 * @param {Analysis} analysis
 * @param {Function} query
 * @param {Function=} queryOptions
 * @return {Array}
 */
function nodeWalkAllDocuments(analysis, query, queryOptions = undefined) {
  const results = [];
  for (const document of analysis.getFeatures({kind: 'html-document'})) {
    const matches = dom5.nodeWalkAll(document.parsedDocument.ast, query, undefined, queryOptions);
    matches.forEach((match) => {
      setNodeFileLocation(match, document);
    });
    results.push(...matches);
  }
  return results;
}

/**
 * Handle the difference between analyzer 2 and 3 Analyzer::getDocument
 * @param {!Analysis} analysis
 * @param {string} url
 * @return {!Document}
 */
function getDocument(analysis, url) {
  const res = analysis.getDocument(url);
  if (res.error) {
    throw res.error;
  }
  if (res.value) {
    return res.value;
  }
  return res;
}

function getAstNode(domModule) {
  if (AnalyzerVersion === '2') {
    return domModule.astNode
  } else {
    return domModule.astNode.node;
  }
}

function getOrderedDomModules(analysis) {
  const domModules = [];
  for (const document of analysis.getFeatures({kind: 'html-document'})) {
    for (const domModule of document.getFeatures({kind: 'dom-module'})) {
      domModules.push(domModule)
    }
  }
  return domModules;
}

async function polymerCssBuild(paths, options = {}) {
  const nativeShadow = options ? !options['build-for-shady'] : true;
  const polymerVersion = options['polymer-version'] || 2;
  const customStyleMatch = polymerVersion === 2 ? customStyleMatchV2 : customStyleMatchV1;
  setUpLibraries(nativeShadow);
  // build analyzer loader
  const loader = new InMemoryOverlayUrlLoader();
  const analyzer = new Analyzer({urlLoader: loader});
  // load given files as strings
  paths.forEach((p) => {
    loader.urlContentsMap.set(analyzer.resolveUrl(p.url), p.content);
  });
  // run analyzer on all given files
  /** @type {Analysis} */
  const analysis = await analyzer.analyze(paths.map((p) => p.url));
  // map dom modules to styles
  const moduleStyles = [];
  for (const domModule of getOrderedDomModules(analysis)) {
    const id = domModule.id;
    const scope = id.toLowerCase();
    const el = getAstNode(domModule);
    domModuleCache[scope] = el;
    setNodeFileLocation(el, domModule);
    markElement(el, scope, nativeShadow, polymerVersion > 1);
    const styles = getAndFixDomModuleStyles(el);
    styles.forEach((s) => {
      scopeMap.set(s, scope);
      setNodeFileLocation(s, domModule);
    });
    moduleStyles.push(styles);
  }
  // inline and flatten styles into a single list
  const flatStyles = [];
  moduleStyles.forEach((styles) => {
    if (!styles.length) {
      return;
    }
    // do style includes
    if (options ? !options['no-inline-includes'] : true) {
      styles.forEach((s) => inlineStyleIncludes(s));
    }
    // reduce styles to one
    const finalStyle = styles[styles.length - 1];
    dom5.setAttribute(finalStyle, 'scope', scopeMap.get(finalStyle));
    if (styles.length > 1) {
      const consumed = styles.slice(0, -1);
      const text = styles.map((s) => dom5.getTextContent(s));
      const includes = styles.map((s) => getAttributeArray(s, 'include')).reduce((acc, inc) => acc.concat(inc));
      consumed.forEach((c) => dom5.remove(c));
      dom5.setTextContent(finalStyle, text.join(''));
      const oldInclude = getAttributeArray(finalStyle, 'include');
      const newInclude = oldInclude.concat(includes).join(' ');
      if (newInclude) {
        dom5.setAttribute(finalStyle, 'include', newInclude);
      }
    }
    flatStyles.push(finalStyle);
  });
  // find custom styles
  const customStyles = nodeWalkAllDocuments(analysis, customStyleMatch);
  // inline custom styles with includes
  if (options ? !options['no-inline-includes'] : true) {
    customStyles.forEach((s) => inlineStyleIncludes(s));
  }
  // add custom styles to the front
  // custom styles may define mixins for the whole tree
  flatStyles.unshift(...customStyles);
  // populate mixin map
  flatStyles.forEach((s) => {
    const text = dom5.getTextContent(s);
    const ast = CssParse.parse(text);
    applyShim(ast);
  });
  // parse, transform, emit
  flatStyles.forEach((s) => {
    let text = dom5.getTextContent(s);
    const ast = CssParse.parse(text);
    if (customStyleMatch(s)) {
      // custom-style `:root` selectors need to be processed to `html`
      StyleUtil.forEachRule(ast, (rule) => {
        if (options && options['build-for-shady']) {
          StyleTransformer.documentRule(rule);
        } else {
          StyleTransformer.normalizeRootSelector(rule);
        }
      });
      // mark the style as built
      markElement(s, null, nativeShadow);
    }
    applyShim(ast);
    if (nativeShadow) {
      if (polymerVersion === 1) {
        slottedTransform(ast);
        dirTransform(ast);
      }
    } else {
      shadyShim(ast, s, analysis);
    }
    text = CssParse.stringify(ast, true);
    dom5.setTextContent(s, text);
  });
  return paths.map((p) => {
    const doc = getDocument(analysis, p.url);
    return {
      url: p.url,
      content: doc.parsedDocument.stringify()
    };
  });
}

exports.polymerCssBuild = polymerCssBuild;
