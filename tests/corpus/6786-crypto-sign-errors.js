// crypto.sign and crypto.verify argument errors in Node's order: the
// algorithm, data and key checks, the signing options read from the key
// argument, KeyObjects of the wrong type, digest names, and the
// RSA-only and EC-only options on the other key type.
import * as crypto from "node:crypto";

const rsaPkcs8 = `-----BEGIN PRIVATE KEY-----
MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAN2n8/miqraNDE/Q
Zj0BK4QF1s9AWodcW6YJDVOYDcwLOSbo628WO5BTw1eqLUuLKXRwIoz4Bsbq942A
urgtvy7TLWOuDXWlMdArSD1X4AMMb7fpUHREvey8JWWnz6RlxrJfd0yKizsZ/TI+
DJCrElPdH636iChnXxeHBSg1kuzZAgMBAAECgYBO12G5prmQK0V40ki7ZEIlOKp6
l4pb4Wcb/NZHqKsnq8pNY8Xygnb3VEQEV5y65Sn+p7Aycjp+4K7jHynLP0Jj7PC+
Mmz1B2IzB8IBKN3qaOrk5OynkWcsi/npzRlhhKYdIFDq1j4vo8ZEy25wR+DFDbpX
Cju4tbEkQnvYAVFBAQJBAPHAVcTmoa38C06tV0q2Z6Pb7nT5LyG+/REkq5xTQspu
SL36IiCgHdxsWM0cH8qgKoywXAMOiygaIDHBpwnKGLkCQQDquGkw1w+o5TNwefyW
eAl7eU/BLO3lQhowOw3kT7BhRBOX9MSvr7mlTV6+RX/MN3ayzoZxbp84J5yox7uV
CyUhAkEAhbyP9ihcIuK+4px0IOm9deFyDhXXpASC/RvbL9TDqzbNWQNqI8BQJKNv
kLBRSgfG4iOiEzLVeq7RRpVJsdmySQJBANkxSU0DRB9R71Zd2qfVbVjO+UjUbVHd
5+PHUWR5hMmMN+E9gh7bqRpXzJqeLtJmZDSyvF8MMNLCfMkFAQO6a2ECQQCWkex8
HGk/Coj3jrpjNxd3sotZ1n/i3v6oCyQs/a92L3s88rLOx1/vCmGrWtYq7hnYKIrV
Vf1O5nJdcOjFHVAz
-----END PRIVATE KEY-----
`;
const prime256v1 = `-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIG47D/R5x59oozeFFKzw2l2Q8iiLfTOWvJvsvgg+dD5voAoGCCqGSM49
AwEHoUQDQgAEWy178rqIN9Zuud1QA7W7OFQ8sj7+RF6Vm7jxf3YGdc5yC2sH/nyu
nOob6jMkc73PRvSGwbEW5p5cVz1zt7q6Rw==
-----END EC PRIVATE KEY-----
`;

function show(label, fn) {
  try {
    const value = fn();
    console.log(label, Buffer.isBuffer(value) ? `Buffer(${value.length})` : JSON.stringify(value));
  } catch (e) {
    console.log(label, "throws", e.name, e.code, JSON.stringify(e.message), Object.keys(e).join());
  }
}

const privateKey = crypto.createPrivateKey(rsaPkcs8);
const publicKey = crypto.createPublicKey(privateKey);
const ec = crypto.createPrivateKey(prime256v1);
const data = Buffer.from("data");
const signature = crypto.sign("sha256", data, privateKey);

show("algorithm number", () => crypto.sign(5, data, privateKey));
show("algorithm object", () => crypto.verify({}, data, publicKey, signature));
show("unknown digest", () => crypto.sign("nope", data, privateKey));
show("unknown digest verify", () => crypto.verify("nope", data, publicKey, signature));
show("digest with spaces", () => crypto.sign("sha256 ", data, privateKey).length);
show("rsa alias with spaces", () => crypto.sign("RSA-SHA256 ", data, privateKey));
show("dss1", () => crypto.sign("dss1", data, privateKey).equals(crypto.sign("sha1", data, privateKey)));
show("data number", () => crypto.sign("sha256", 5, privateKey));
show("data undefined", () => crypto.sign("sha256", undefined, privateKey));
show("data array buffer", () => crypto.sign("sha256", /** @type {any} */ (new Uint8Array([1, 2]).buffer), privateKey));
show("data typed array", () => crypto.sign("sha256", new Uint16Array([1, 2]), privateKey));
show("data data view", () => crypto.verify("sha256", new DataView(data.buffer, data.byteOffset, data.length), publicKey, signature));
show("key missing", () => crypto.sign("sha256", data));
show("key empty string", () => crypto.sign("sha256", data, ""));
show("key zero", () => crypto.sign("sha256", data, 0));
show("key number", () => crypto.sign("sha256", data, 5));
show("key boolean", () => crypto.sign("sha256", data, true));
show("key empty object", () => crypto.sign("sha256", data, {}));
show("key null option", () => crypto.sign("sha256", data, { key: null }));
show("public KeyObject", () => crypto.sign("sha256", data, publicKey));
show("public KeyObject option", () => crypto.sign("sha256", data, { key: publicKey }));
show("secret KeyObject", () => crypto.sign("sha256", data, crypto.createSecretKey(Buffer.alloc(4))));
show("spki pem", () => crypto.sign("sha256", data, publicKey.export({ format: "pem", type: "spki" })));
show("verify undefined key", () => crypto.verify("sha256", data, undefined, signature));
show("verify null key", () => crypto.verify("sha256", data, null, signature));
show("verify number key", () => crypto.verify("sha256", data, 5, signature));
show("verify secret key", () => crypto.verify("sha256", data, crypto.createSecretKey(Buffer.alloc(4)), signature));
show("verify missing signature", () => crypto.verify("sha256", data, publicKey));
show("verify string signature", () => crypto.verify("sha256", data, publicKey, signature.toString("latin1")));
show("verify array buffer signature", () => crypto.verify("sha256", data, publicKey, /** @type {any} */ (new Uint8Array(signature).buffer)));
show("padding float", () => crypto.sign("sha256", data, { key: privateKey, padding: 1.5 }));
show("padding null", () => crypto.sign("sha256", data, { key: privateKey, padding: null }));
show("saltLength string", () => crypto.sign("sha256", data, { key: privateKey, padding: 6, saltLength: "1" }));
show("saltLength large", () => crypto.sign("sha256", data, { key: privateKey, padding: 6, saltLength: 2 ** 31 }));
show("dsaEncoding number", () => crypto.sign("sha256", data, { key: ec, dsaEncoding: 1 }));
show("dsaEncoding null", () => crypto.sign("sha256", data, { key: ec, dsaEncoding: null }));
show("dsaEncoding undefined", () => crypto.sign("sha256", data, { key: ec, dsaEncoding: undefined }).length > 60);
show("option order", () => crypto.sign("sha256", data, { key: 5, padding: 1.5 }));
show("option order verify", () => crypto.verify("sha256", data, { key: publicKey, dsaEncoding: "x" }, 5));
show("callback not a function", () => crypto.sign("sha256", data, privateKey, 5));
show("verify callback not a function", () => crypto.verify("sha256", data, publicKey, signature, "x"));
