// Key input and export errors: createPrivateKey and createPublicKey over
// malformed PEM and DER, wrong argument types, KeyObjects of the wrong
// type and invalid encoding options, and export() option errors, with
// Node's error classes, codes, messages and OpenSSL properties.
import * as crypto from "node:crypto";
const { createPrivateKey, createPublicKey, createSecretKey } = crypto;

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
    console.log(label, value instanceof crypto.KeyObject ? value.type : value);
  } catch (e) {
    console.log(label, "throws", e.name, e.code, JSON.stringify(e.message));
    if (e.opensslErrorStack) console.log("  ", JSON.stringify(e.opensslErrorStack), e.library, e.reason);
  }
}

const privateKey = createPrivateKey(rsaPkcs8);
const publicKey = createPublicKey(privateKey);
const der = privateKey.export({ format: "der", type: "pkcs8" });

show("garbage", () => createPrivateKey("garbage"));
show("garbage public", () => createPublicKey("garbage"));
show("empty", () => createPrivateKey(""));
show("truncated body", () => createPrivateKey(rsaPkcs8.slice(0, 100) + "\n-----END PRIVATE KEY-----\n"));
show("bad base64", () => createPrivateKey("-----BEGIN PRIVATE KEY-----\n!!!!\n-----END PRIVATE KEY-----\n"));
show("no end line", () => createPrivateKey(rsaPkcs8.slice(0, 200)));
show("wrong label", () => createPrivateKey(rsaPkcs8.replace(/PRIVATE KEY/g, "EC PRIVATE KEY")));
show("leading text", () => createPrivateKey("text before\n" + rsaPkcs8));
show("crlf", () => createPrivateKey(rsaPkcs8.replace(/\n/g, "\r\n")));
show("spki as private", () => createPrivateKey(publicKey.export({ format: "pem", type: "spki" })));
show("der garbage", () => createPrivateKey({ key: Buffer.from([1, 2, 3]), format: "der", type: "pkcs8" }));
show("der without type", () => createPrivateKey({ key: der, format: "der" }));
show("der spki type", () => createPrivateKey({ key: der, format: "der", type: "spki" }));
show("der sec1 type for pkcs1", () => createPrivateKey({ key: privateKey.export({ format: "der", type: "pkcs1" }), format: "der", type: "sec1" }));
show("der pkcs8 type for pkcs1", () => createPrivateKey({ key: privateKey.export({ format: "der", type: "pkcs1" }), format: "der", type: "pkcs8" }));
show("public der garbage", () => createPublicKey({ key: Buffer.from([1, 2, 3]), format: "der", type: "spki" }));
show("public der pkcs1 garbage", () => createPublicKey({ key: Buffer.from([1, 2, 3]), format: "der", type: "pkcs1" }));
show("public der pkcs8 garbage", () => createPublicKey({ key: Buffer.from([1]), format: "der", type: "pkcs8" }));
show("public pem of private", () => createPublicKey({ key: rsaPkcs8, format: "pem" }));
show("array buffer", () => createPrivateKey(new Uint8Array(Buffer.from(rsaPkcs8)).buffer));
show("data view", () => createPrivateKey(new DataView(new Uint8Array(Buffer.from(rsaPkcs8)).buffer)));

show("number", () => createPrivateKey(5));
show("undefined", () => createPrivateKey());
show("null", () => createPrivateKey(null));
show("public null", () => createPublicKey(null));
show("boolean public", () => createPublicKey(true));
show("empty object", () => createPrivateKey({}));
show("key number", () => createPrivateKey({ key: 5 }));
show("public key number", () => createPublicKey({ key: 5 }));
show("bad format", () => createPrivateKey({ key: rsaPkcs8, format: "xyz" }));
show("bad type", () => createPrivateKey({ key: rsaPkcs8, type: "bogus" }));
show("spki type on private", () => createPrivateKey({ key: rsaPkcs8, type: "spki" }));
show("pkcs1 type on pem", () => createPrivateKey({ key: rsaPkcs8, format: "pem", type: "pkcs1" }));
show("pkcs8 type public pem", () => createPublicKey({ key: rsaPkcs8, type: "pkcs8" }));
show("unknown encoding", () => createPrivateKey({ key: rsaPkcs8, encoding: "nope" }));
show("passphrase on plain key", () => createPrivateKey({ key: rsaPkcs8, passphrase: "x" }));
show("passphrase number", () => createPrivateKey({ key: rsaPkcs8, passphrase: 5 }));
show("private KeyObject to createPrivateKey", () => createPrivateKey(privateKey));
show("public KeyObject to createPrivateKey", () => createPrivateKey(publicKey));
show("KeyObject in key", () => createPrivateKey({ key: privateKey }));
show("public KeyObject to createPublicKey", () => createPublicKey(publicKey));
show("secret KeyObject to createPublicKey", () => createPublicKey(createSecretKey(Buffer.alloc(4))));
show("private KeyObject in key", () => createPublicKey({ key: privateKey }));
show("jwk string", () => createPrivateKey({ key: "x", format: "jwk" }));
show("jwk no kty", () => createPrivateKey({ key: {}, format: "jwk" }));
show("jwk bad kty", () => createPrivateKey({ key: { kty: "XX" }, format: "jwk" }));
show("jwk oct", () => createPrivateKey({ key: { kty: "oct", k: "AAAA" }, format: "jwk" }));
show("jwk rsa without n", () => createPublicKey({ key: { kty: "RSA", e: "AQAB" }, format: "jwk" }));
const jwk = privateKey.export({ format: "jwk" });
show("jwk rsa partial private", () => createPrivateKey({ key: { kty: "RSA", n: jwk.n, e: jwk.e, d: jwk.d }, format: "jwk" }));
show("jwk rsa public as private", () => createPrivateKey({ key: { kty: "RSA", n: jwk.n, e: jwk.e }, format: "jwk" }));
show("raw-public into private", () => createPrivateKey({ key: Buffer.alloc(65), format: "raw-public", asymmetricKeyType: "ec", namedCurve: "prime256v1" }));

show("export no options", () => privateKey.export());
show("export string", () => privateKey.export("pem"));
show("export null", () => privateKey.export(null));
show("export public no options", () => publicKey.export());
show("export pem no type", () => privateKey.export({ format: "pem" }));
show("export public no type", () => publicKey.export({ format: "pem" }));
show("export bad format", () => privateKey.export({ format: "xyz", type: "pkcs8" }));
show("export spki on private", () => privateKey.export({ format: "pem", type: "spki" }));
show("export pkcs8 on public", () => publicKey.export({ format: "pem", type: "pkcs8" }));
show("export sec1 on rsa", () => privateKey.export({ format: "pem", type: "sec1" }));
show("export passphrase without cipher", () => privateKey.export({ format: "pem", type: "pkcs8", passphrase: "x" }));
show("export jwk passphrase", () => privateKey.export({ format: "jwk", passphrase: "x" }));
show("export cipher number", () => privateKey.export({ format: "pem", type: "pkcs8", cipher: 5, passphrase: "x" }));
show("export der pkcs1 cipher", () => privateKey.export({ format: "der", type: "pkcs1", cipher: "aes-256-cbc", passphrase: "x" }));
show("export cipher without passphrase", () => privateKey.export({ format: "pem", type: "pkcs8", cipher: "aes-256-cbc" }));
show("export rsa raw-public", () => publicKey.export({ format: "raw-public" }));
show("export rsa raw-private", () => privateKey.export({ format: "raw-private" }));
show("set type", () => { privateKey.type = "public"; });
show("set asymmetricKeyType", () => { publicKey.asymmetricKeyType = "ec"; });
show("set asymmetricKeyDetails", () => { publicKey.asymmetricKeyDetails = {}; });
show("details copy", () => { const d = privateKey.asymmetricKeyDetails; d.modulusLength = 1; return privateKey.asymmetricKeyDetails.modulusLength; });
show("symmetricKeySize", () => privateKey.symmetricKeySize);
