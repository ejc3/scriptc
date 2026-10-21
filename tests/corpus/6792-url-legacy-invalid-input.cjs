// @exit: 1
// url.parse()'s ERR_INVALID_URL carries the url it was given as `input`, as
// an own enumerable property after `code`, from url.parse(), url.resolve(),
// url.resolveObject() and Url.prototype.parse(). The @exit directive keeps
// the DEP0169 report (its pid never matches) out of the stderr comparison.
'use strict';
const url = require('url');

const inputs = ['http://a\u0000b/', 'http://[::1', 'http://exa mple.com/', 'https://[::1]x/', 'http://a.b%25/'];
const calls = [
  { label: 'parse', call: (input) => url.parse(input) },
  { label: 'resolve', call: (input) => url.resolve(input, 'x') },
  { label: 'resolve relative', call: (input) => url.resolve('http://ok/', input) },
  { label: 'resolveObject', call: (input) => url.resolveObject(input, 'x') },
  { label: 'Url#parse', call: (input) => new url.Url().parse(input) },
];
for (const input of inputs) {
  for (const { label, call } of calls) {
    try {
      const result = call(input);
      console.log(label, JSON.stringify(input), 'ok', typeof result === 'string' ? result : result.href);
    } catch (e) {
      console.log(label, JSON.stringify(input), e.name, e.code, e.message, JSON.stringify(e.input), Object.keys(e).join(','));
    }
  }
}
process.exitCode = 1;
