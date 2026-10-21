// @exit: 1
// node:url's Url class from CommonJS: new url.Url() with its twelve null parts,
// instanceof, Url.prototype.parse() into a new Url (with parseQueryString and
// slashesDenoteHost), parseHost() on a host with and without a port, the
// TypeErrors of format() and parseHost() without a receiver,
// url.resolveObject()'s falsy-source shortcut, and the TypeErrors of a
// non-string url and an undefined relative. The @exit directive keeps the
// DEP0169 report (its pid never matches) out of the stderr comparison.
'use strict';
const url = require('url');
const util = require('util');

const attempt = (label, f) => {
  try {
    console.log(label, util.inspect(f()));
  } catch (e) {
    console.log(label, e.name, e.code, e.message);
  }
};

const empty = new url.Url();
console.log(util.inspect(empty), Object.keys(empty).join(','));
console.log(empty instanceof url.Url, url.parse('x') instanceof url.Url, {} instanceof url.Url);
console.log(JSON.stringify(empty), empty.format());

const into = new url.Url();
const back = into.parse('https://user:pw@Z.Example:8443/a b/c?q=1&q=2&r#top', true);
console.log(back === into, into.host, into.hostname, into.port, into.pathname, into.hash);
console.log(util.inspect(into.query), into.href);
const host = new url.Url().parse('//h.example/p', false, true);
console.log(host.host, host.pathname, host.slashes, new url.Url().parse('//h.example/p').host);

const hosts = ['h.example:9090', 'h.example:', 'h.example', ':80', ':', '[::1]:8080', 'h:12:34'];
for (const value of hosts) {
  const u = url.parse('http://placeholder/');
  u.host = value;
  u.hostname = 'before';
  u.port = 'before';
  u.parseHost();
  console.log(JSON.stringify(value), JSON.stringify(u.hostname), JSON.stringify(u.port));
}
const unset = new url.Url();
unset.parseHost();
console.log(unset.hostname, unset.port);

console.log(url.resolveObject('', 'foo'), url.resolveObject(null, 'bar'), url.resolveObject(undefined, 3));
const kept = url.parse('http://a/b');
console.log(url.resolveObject(0, kept) === kept);
attempt('resolveObject(1, "a")', () => url.resolveObject(1, 'a'));
attempt('resolveObject("http://a/", undefined)', () => url.resolveObject('http://a/', undefined));
attempt('resolveObject("http://a/", null)', () => url.resolveObject('http://a/', null));
attempt('Url#parse(42)', () => new url.Url().parse(42));
attempt('Url#resolve(42)', () => url.parse('http://a/').resolve(42));
attempt('Url#resolveObject()', () => url.parse('http://a/').resolveObject());
const { format, parseHost } = url.parse('http://a/');
attempt('Url#format on undefined', () => format.call(undefined));
attempt('Url#format on null', () => format.call(null));
attempt('Url#parseHost on undefined', () => parseHost.call(undefined));
process.exitCode = 1;
