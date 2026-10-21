// Sign and Verify objects: createSign and createVerify with update() over
// strings in each encoding and over views, sign() and verify() with every
// key form and output and signature encodings, the spent-object errors,
// and the algorithm and argument errors.
"use strict";
const crypto = require("node:crypto");

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
    console.log(label, Buffer.isBuffer(value) ? "Buffer " + value.toString("hex") : JSON.stringify(value));
  } catch (e) {
    console.log(label, "throws", e.name, e.code, JSON.stringify(e.message));
  }
}

const privateKey = crypto.createPrivateKey(rsaPkcs8);
const publicKey = crypto.createPublicKey(privateKey);
const ec = crypto.createPrivateKey(prime256v1);
const ecPublic = crypto.createPublicKey(ec);
const message = "message to sign";

const signer = crypto.createSign("sha256");
show("update returns this", () => signer.update("message ") === signer);
show("update hex", () => signer.update(Buffer.from("to ").toString("hex"), "hex") === signer);
show("update view", () => signer.update(new Uint8Array(Buffer.from("sign"))) === signer);
const signature = signer.sign(privateKey);
show("matches one-shot", () => signature.equals(crypto.sign("sha256", Buffer.from(message), privateKey)));
show("sign again", () => signer.sign(privateKey));
show("update after sign", () => signer.update("x"));

for (const encoding of ["hex", "base64", "base64url", "latin1", "buffer", undefined, ""]) {
  show(`sign ${encoding}`, () => {
    const s = crypto.createSign("RSA-SHA256");
    s.update(message);
    const out = s.sign(privateKey, encoding);
    return Buffer.isBuffer(out) ? out.length : out.slice(0, 24);
  });
}
show("sign unknown encoding", () => crypto.createSign("sha256").update(message).sign(privateKey, "nope"));

const pssOptions = { key: publicKey, padding: crypto.constants.RSA_PKCS1_PSS_PADDING };
for (const [label, key, verifyKey] of [
  ["pem", rsaPkcs8, publicKey],
  ["buffer", Buffer.from(rsaPkcs8), publicKey],
  ["object", { key: rsaPkcs8 }, publicKey],
  ["key object option", { key: privateKey }, publicKey],
  ["pss", { key: privateKey, padding: crypto.constants.RSA_PKCS1_PSS_PADDING, saltLength: 20 }, pssOptions],
]) {
  show(`sign with ${label}`, () => {
    const s = crypto.createSign("sha256");
    s.update(message);
    const out = s.sign(key);
    const v = crypto.createVerify("sha256");
    v.update(message);
    return [out.length, v.verify(verifyKey, out)];
  });
}

const verifier = crypto.createVerify("sha256");
show("verify update returns this", () => verifier.update(message) === verifier);
show("verify", () => verifier.verify(publicKey, signature));
show("verify again", () => verifier.verify(publicKey, signature));
show("verify update after", () => verifier.update("x"));
for (const encoding of ["hex", "base64", "base64url", "latin1", "buffer", undefined]) {
  show(`verify ${encoding}`, () => {
    const v = crypto.createVerify("sha256");
    v.update(message);
    return v.verify(publicKey, encoding === undefined || encoding === "buffer" ? signature : signature.toString(encoding), encoding);
  });
}
show("verify string without encoding", () => crypto.createVerify("sha256").update(message).verify(publicKey, signature.toString("latin1")));
show("verify with private key", () => crypto.createVerify("sha256").update(message).verify(privateKey, signature));
show("verify with pem", () => crypto.createVerify("sha256").update(message).verify(rsaPkcs8, signature));
show("verify wrong message", () => crypto.createVerify("sha256").update("other").verify(publicKey, signature));
show("verify missing signature", () => crypto.createVerify("sha256").update(message).verify(publicKey));
show("verify number signature", () => crypto.createVerify("sha256").update(message).verify(publicKey, 5));

const ecSigner = crypto.createSign("sha384");
ecSigner.update(message);
const ecSignature = ecSigner.sign({ key: ec, dsaEncoding: "ieee-p1363" });
show("ec p1363 length", () => ecSignature.length);
show("ec p1363 verify", () => crypto.createVerify("sha384").update(message).verify({ key: ecPublic, dsaEncoding: "ieee-p1363" }, ecSignature));
show("ec p1363 as der", () => crypto.createVerify("sha384").update(message).verify(ecPublic, ecSignature));
show("ec malformed p1363", () => crypto.createVerify("sha384").update(message).verify({ key: ecPublic, dsaEncoding: "ieee-p1363" }, ecSignature.subarray(1)));
show("ec der", () => {
  const der = crypto.createSign("sha384").update(message).sign(ec);
  return [der[0], crypto.createVerify("sha384").update(message).verify(ecPublic, der)];
});

show("createSign unknown", () => crypto.createSign("nope"));
show("createVerify unknown", () => crypto.createVerify("nope"));
show("createSign number", () => crypto.createSign(5));
show("createSign none", () => crypto.createSign());
show("createSign upper", () => crypto.createSign("SHA512").update("x").sign(privateKey).length);
show("createSign shake", () => crypto.createSign("shake256").update("x").sign(privateKey));
show("update number", () => crypto.createSign("sha256").update(5));
show("update odd hex", () => crypto.createSign("sha256").update("abc", "hex"));
show("update unknown encoding", () => crypto.createSign("sha256").update(message, "nope").sign(privateKey).equals(signature));
show("sign no key", () => crypto.createSign("sha256").update(message).sign());
show("sign empty key", () => crypto.createSign("sha256").update(message).sign(""));
show("sign public key", () => crypto.createSign("sha256").update(message).sign(publicKey));
show("sign spki pem", () => crypto.createSign("sha256").update(message).sign(publicKey.export({ format: "pem", type: "spki" })));
show("sign number key", () => crypto.createSign("sha256").update(message).sign(5));
show("verify secret key", () => crypto.createVerify("sha256").update(message).verify(crypto.createSecretKey(Buffer.alloc(4)), signature));
show("verify number key", () => crypto.createVerify("sha256").update(message).verify(5, signature));
show("sign after failed sign", () => {
  const s = crypto.createSign("sha256");
  s.update(message);
  try { s.sign(publicKey); } catch (e) { console.log("  first", e.code); }
  return s.sign(privateKey).length;
});
