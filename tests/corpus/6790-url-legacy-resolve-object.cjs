// @exit: 1
// The legacy url resolvers on Url records, from CommonJS: url.resolveObject()
// over a parsed Url and over a string, Url.prototype.resolveObject() and
// resolve() on url.parse() results, and the result's twelve parts and href,
// for the relative cases of Node's test-url-relative.js. The @exit directive
// keeps the DEP0169 report (its pid never matches) out of the stderr
// comparison.
'use strict';
const url = require('url');
const util = require('util');

// [from, path, expected]
const relativeTests = [
  ['/foo/bar/baz', 'quux', '/foo/bar/quux'],
  ['/foo/bar/baz', 'quux/asdf', '/foo/bar/quux/asdf'],
  ['/foo/bar/baz', 'quux/baz', '/foo/bar/quux/baz'],
  ['/foo/bar/baz', '../quux/baz', '/foo/quux/baz'],
  ['/foo/bar/baz', '/bar', '/bar'],
  ['/foo/bar/baz/', 'quux', '/foo/bar/baz/quux'],
  ['/foo/bar/baz/', 'quux/baz', '/foo/bar/baz/quux/baz'],
  ['/foo/bar/baz', '../../../../../../../../quux/baz', '/quux/baz'],
  ['/foo/bar/baz', '../../../../../../../quux/baz', '/quux/baz'],
  ['/foo', '.', '/'],
  ['/foo', '..', '/'],
  ['/foo/', '.', '/foo/'],
  ['/foo/', '..', '/'],
  ['/foo/bar', '.', '/foo/'],
  ['/foo/bar', '..', '/'],
  ['/foo/bar/', '.', '/foo/bar/'],
  ['/foo/bar/', '..', '/foo/'],
  ['foo/bar', '../../../baz', '../../baz'],
  ['foo/bar/', '../../../baz', '../baz'],
  ['http://example.com/b//c//d;p?q#blarg', 'https:#hash2', 'https:///#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'https:/p/a/t/h?s#hash2',
   'https://p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'https://u:p@h.com/p/a/t/h?s#hash2',
   'https://u:p@h.com/p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'https:/a/b/c/d',
   'https://a/b/c/d'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http:#hash2',
   'http://example.com/b//c//d;p?q#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http:/p/a/t/h?s#hash2',
   'http://example.com/p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http://u:p@h.com/p/a/t/h?s#hash2',
   'http://u:p@h.com/p/a/t/h?s#hash2'],
  ['http://example.com/b//c//d;p?q#blarg',
   'http:/a/b/c/d',
   'http://example.com/a/b/c/d'],
  ['/foo/bar/baz', '/../etc/passwd', '/etc/passwd'],
  ['http://localhost', 'file:///Users/foo', 'file:///Users/foo'],
  ['http://localhost', 'file://foo/Users', 'file://foo/Users'],
  ['https://registry.npmjs.org', '@foo/bar', 'https://registry.npmjs.org/@foo/bar'],
];

const parts = (u) =>
  [u.protocol, u.slashes, u.auth, u.host, u.port, u.hostname, u.hash, u.search, u.query,
    u.pathname, u.path, u.href].map((part) => JSON.stringify(part)).join(' ');

for (const [from, to, expected] of relativeTests) {
  const parsed = url.parse(from);
  const viaModule = url.resolveObject(parsed, to);
  const viaString = url.resolveObject(from, to);
  const viaMethod = parsed.resolveObject(to);
  const resolved = parsed.resolve(to);
  console.log(JSON.stringify([from, to]), resolved === expected, url.format(viaModule) === expected);
  console.log('  ', parts(viaModule));
  console.log('  ', parts(viaString) === parts(viaModule), parts(viaMethod) === parts(viaModule));
}

// The result is a new Url; the receiver and a Url argument are unchanged.
const base = url.parse('http://user@a.example:8080/b/c?d=e#f');
const relative = url.parse('../g?h#i');
const before = parts(base) + parts(relative);
const result = base.resolveObject(relative);
console.log(result !== base, result !== relative, result instanceof url.Url, before === parts(base) + parts(relative));
console.log(util.inspect(result));
console.log(Object.keys(result).join(','));
console.log(base.resolve(relative), base.resolve('?only=query'), base.resolve(''), base.resolve('#frag'));

// A parsed query object travels into the result as the same object.
const withQuery = url.parse('http://a/b?c=1&c=2', true);
const kept = withQuery.resolveObject('#x');
console.log(kept.query === withQuery.query, util.inspect(kept.query), kept.href);
process.exitCode = 1;
