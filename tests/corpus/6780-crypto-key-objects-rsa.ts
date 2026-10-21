// Public and private RSA KeyObjects: createPrivateKey and createPublicKey
// over PEM and DER input in every encoding, the KeyObject getters,
// export() to pkcs1, pkcs8, spki, DER and JWK, JWK input, equals(), and
// how the keys print.
import { createPrivateKey, createPublicKey, createSecretKey, KeyObject } from "node:crypto";
import { inspect } from "node:util";

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
const rsa3Pkcs1 = `-----BEGIN RSA PRIVATE KEY-----
MIICXwIBAAKBggDtZMe0/8DjPZNuLG8k4Wj7kiRpSvOzN8pSjcqtIc9mHjSYew6n
mzVsdb+v8F1+vJvoNj7jvv2dF/puDFCGkCZH2tXM1RI3ZvVANsqg1px5d2W4j/WG
8tlfYe6iD4sT9pckYxGxMuwroEPlIp5Dey1Xz8HgQbxiXUy+pv9uPTHW+7kCAQMC
gYIAnkMvzf/V7NO3nshKGJZF/QwYRjH3zM/cNwkxyME07r7Nuvy0b7zOSE5/yqA+
VH29RXl/Qn9Tvg/8SV2LBGAZhSgDOcYNA3jQ7ghgppk9tlAcSkCNPSd33bY/yUCt
qfK8Msel+KLQSOIZo7IvQykNEMnou3+tRkZHqQCUkiMxb1+rAkEPiZp7VwULbse0
qZ6kl5AA2/X5c8jYkeAZL2NMR2e0YDQiYHAaVS7E5j23mqPDM3cz32xEL0aIhC/H
tvoqrMVUyQJBD0dbsKetJk0TdZAITChX/l9TNa3iXxOyt1+Q4j8tVhykFThMJF6P
iDcx37y6+mZHbQVavgkya10RXmmY3brql3ECQQpbvFI6A1z0hSMbvxhlCqs9TqZN
MJBhQBDKQjLaRSLqzWxASrw4ydiZfnpnF9d3pM0/nYLKLwWtdS/PUXHIg43bAkEK
L5J1xR4ZiLejtVrdcDqplOIjyUGUt8x6P7XsKh45aG1jet1tlF+wJMvqfdH8RC+e
A5HUBiGc6LY+8RCT0fG6SwJBCMcD6S0DFdkeS2D2aFtqQWG+YH5Xja0g5tTqmp/h
cI2g+wAKAFt3CDcLBKJNNZ0OcA0wvR43aPrjpZDT0Fml580=
-----END RSA PRIVATE KEY-----
`;

function show(label: string, fn: () => unknown): void {
  try {
    const value = fn();
    console.log(label, typeof value === "string" ? JSON.stringify(value) : Buffer.isBuffer(value) ? value.toString("hex") : inspect(value));
  } catch (e) {
    const error = e as Error & { code?: string };
    console.log(label, "throws", error.name, error.code, error.message);
  }
}

const privateKey = createPrivateKey(rsaPkcs8);
console.log(privateKey.type, privateKey.asymmetricKeyType, privateKey.symmetricKeySize);
console.log(privateKey.asymmetricKeyDetails);
console.log(privateKey instanceof KeyObject, inspect(privateKey), String(privateKey), JSON.stringify(privateKey));

const publicKey = createPublicKey(privateKey);
console.log(publicKey.type, publicKey.asymmetricKeyType, publicKey.asymmetricKeyDetails);
console.log(inspect(publicKey), publicKey instanceof KeyObject);

const pkcs1 = privateKey.export({ format: "pem", type: "pkcs1" }) as string;
const pkcs8 = privateKey.export({ format: "pem", type: "pkcs8" }) as string;
const spki = publicKey.export({ format: "pem", type: "spki" }) as string;
const pkcs1Public = publicKey.export({ format: "pem", type: "pkcs1" }) as string;
console.log(pkcs8 === rsaPkcs8);
console.log(pkcs1);
console.log(spki);
console.log(pkcs1Public);
const pkcs8Der = privateKey.export({ format: "der", type: "pkcs8" });
const pkcs1Der = privateKey.export({ format: "der", type: "pkcs1" });
const spkiDer = publicKey.export({ format: "der", type: "spki" });
const pkcs1PublicDer = publicKey.export({ format: "der", type: "pkcs1" });
console.log(pkcs8Der.length, pkcs1Der.length, spkiDer.length, pkcs1PublicDer.length);
console.log(spkiDer.toString("base64"));

// Every input encoding reads back the same key.
const inputs: [string, () => KeyObject][] = [
  ["pkcs1 pem", () => createPrivateKey(pkcs1)],
  ["pkcs8 buffer", () => createPrivateKey(Buffer.from(pkcs8))],
  ["pkcs8 der", () => createPrivateKey({ key: pkcs8Der, format: "der", type: "pkcs8" })],
  ["pkcs1 der", () => createPrivateKey({ key: pkcs1Der, format: "der", type: "pkcs1" })],
  ["base64 der", () => createPrivateKey({ key: pkcs8Der.toString("base64"), format: "der", type: "pkcs8", encoding: "base64" })],
  ["spki pem", () => createPublicKey(spki)],
  ["pkcs1 public pem", () => createPublicKey(pkcs1Public)],
  ["spki der", () => createPublicKey({ key: spkiDer, format: "der", type: "spki" })],
  ["pkcs1 public der", () => createPublicKey({ key: pkcs1PublicDer, format: "der", type: "pkcs1" })],
  ["public from private pem", () => createPublicKey(rsaPkcs8)],
  ["public from private der", () => createPublicKey({ key: pkcs1Der, format: "der", type: "pkcs1" })],
];
for (const [label, make] of inputs) {
  show(label, () => {
    const key = make();
    return [key.type, key.equals(key.type === "private" ? privateKey : publicKey), key.asymmetricKeyDetails?.modulusLength];
  });
}

const jwk = privateKey.export({ format: "jwk" });
console.log(Object.keys(jwk).join(), jwk.kty, jwk.e);
console.log(publicKey.export({ format: "jwk" }));
const fromJwk = createPrivateKey({ key: jwk, format: "jwk" });
console.log(fromJwk.equals(privateKey), fromJwk.export({ format: "pem", type: "pkcs1" }) === pkcs1);
const publicFromJwk = createPublicKey({ key: { kty: "RSA", n: jwk.n, e: jwk.e }, format: "jwk" });
console.log(publicFromJwk.type, publicFromJwk.equals(publicKey));

// A public exponent other than 65537.
const small = createPrivateKey(rsa3Pkcs1);
console.log(small.asymmetricKeyDetails, small.export({ format: "jwk" }).e);
console.log(createPublicKey(small).export({ format: "pem", type: "spki" }));

console.log(privateKey.equals(publicKey), publicKey.equals(privateKey), privateKey.equals(small));
console.log(privateKey.equals(createSecretKey(Buffer.alloc(8))), createSecretKey(Buffer.alloc(8)).equals(publicKey));
show("equals string", () => privateKey.equals("key" as unknown as KeyObject));
