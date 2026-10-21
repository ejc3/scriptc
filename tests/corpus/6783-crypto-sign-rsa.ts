// RSA signatures: crypto.sign and crypto.verify with PKCS#1 v1.5 padding
// over every digest OpenSSL's RSA provider allows (the signatures are
// deterministic, so they print), PSS padding with OpenSSL's salt length
// rules, verification of signatures Node made, and the padding, salt and
// digest errors.
import { constants, createPrivateKey, createPublicKey, getHashes, sign, verify } from "node:crypto";

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
const pssSha256Salt32 = "5a7d9f47aea437a5382e587eea01b93740a45f676d4cd1698b72aecac2f34fe763f0f3059ee73d1ca4cbded2a17e8d426c1f2f47d3d6e18ba9984f15e5c87a43021aa1a43c535b376a31b97ba84fe42b1ab6d30178b7f1d5b3138a4f0173af1cb8c6a73dd72c0d9aab0a5dbc276bb812182b7257a6964606e8350de0e7274069";
const pssSha1SaltMax = "bbb1f617ba9c59c7230f33a6e6ea4fd5f77032ce124dac62b6f71dc6ab87d7ff3b3e3a7e27f7cebe76a94f8a57986946dd244bb13d201765c1fd828e60f404ac8f4440c7831fe21a553080155f6658fe52344b5ec54192b6d24ddcce462dc3ee46da967cb6d8cfdde87cac3d40386b366cdb852449fb97d5a3b7f6643cd706d4";
const pssSha512Salt0 = "2febf001718b01c7e251b2538de8442aacb4a9f1a259e15dd60f04d42e0f9276c9a8ba3c11ccbd3bf43a63aae5594a6f64ee83e31b5eacb23e4d998971dad302c59c47e8b9d21596abbb557d132c5c4e3b9e829f799a3ab523712d7a6ae3ea0c57c5694fae30755e69435b0d937ddff1cd6f7b13726224ef1ab8ca6de85c2b52";
const p256Der = "3045022100d99ed94adfbc52d6c8067843ae2d838a2f0b2ced79c23e435de48d0753982c0b02201b4c0ca0785c267b26326e292b34b12fdac50b090708752a1e551b7d02d1d399";
const p256P1363 = "e02e27e22b96260de7f0f622029b0aa1a3bdc76deab08b9a9eff680ede0e4aaf47ad85d8819fa013de288fa71a239bd7a1ed471f11f333f0bbe34ecfe1dc6e3a";
const k1Der = "30450220286701911a5c6d2970f1c0b247ffe82d2fe46f2c6308e398477df96bfa7700ca022100ac308c3be8fe193c9f9954cc70f16fe613b1c17015b3b70ae0216d6fc0796ee4";
const p521Der = "308186024169fc30d4d945c189ef5cac3cbe593fd19d6da39037fd4245aa3355ee58f2ea0c8373187346a453e64932588950b576cc8cfde34594849caf91a61d2c9c8bbc56fc024179e8d306fb0ed615678dac4e47193eedd8277e0e9ba0f60958a5620317fd4f2eaae592feea357c8c62c531c981f8b7a21453bd10587ed8abbba80c4015e82466b9";

function show(label: string, fn: () => unknown): void {
  try {
    const value = fn();
    console.log(label, Buffer.isBuffer(value) ? value.toString("hex") : JSON.stringify(value));
  } catch (e) {
    const error = e as Error & { code?: string };
    console.log(label, "throws", error.name, error.code, error.message);
  }
}

const privateKey = createPrivateKey(rsaPkcs8);
const publicKey = createPublicKey(privateKey);
const data = Buffer.from("signed by Node");

for (const name of getHashes()) {
  show(name, () => {
    const signature = sign(name, data, privateKey);
    return [signature.toString("base64"), verify(name, data, publicKey, signature)];
  });
}
show("null algorithm", () => sign(null, data, privateKey).equals(sign("sha256", data, privateKey)));
show("undefined algorithm", () => verify(undefined, data, publicKey, sign("SHA256", data, privateKey)));
show("string data", () => sign("sha256", "signed by Node" as unknown as Buffer, privateKey).equals(sign("sha256", data, privateKey)));
show("pem key", () => sign("sha256", data, rsaPkcs8).equals(sign("sha256", data, privateKey)));
show("key option", () => sign("sha256", data, { key: rsaPkcs8, padding: constants.RSA_PKCS1_PADDING }).length);
show("verify with private key", () => verify("sha256", data, privateKey, sign("sha256", data, privateKey)));
show("wrong data", () => verify("sha256", Buffer.from("other"), publicKey, sign("sha256", data, privateKey)));
show("short signature", () => verify("sha256", data, publicKey, Buffer.alloc(10)));
show("long signature", () => verify("sha256", data, publicKey, Buffer.alloc(129)));
show("signature too big", () => verify("sha256", data, publicKey, Buffer.alloc(128, 0xff)));

// PSS: random salts, so the signatures verify rather than print.
const pss = (saltLength?: number) =>
  sign("sha256", data, { key: privateKey, padding: constants.RSA_PKCS1_PSS_PADDING, saltLength });
for (const salt of [undefined, -1, -2, -3, -4, 0, 1, 32, 94]) {
  show(`pss salt ${salt}`, () => {
    const signature = pss(salt);
    return [signature.length, verify("sha256", data, { key: publicKey, padding: 6 }, signature),
      verify("sha256", data, { key: publicKey, padding: 6, saltLength: salt ?? -2 }, signature),
      verify("sha256", data, publicKey, signature)];
  });
}
show("pss salt 95", () => pss(95));
show("pss salt -5", () => pss(-5));
for (const [salt, against] of [[32, -1], [32, -3], [94, -3], [10, 10], [10, 9], [0, 0], [94, -4], [32, -4]] as const) {
  show(`pss ${salt} against ${against}`, () => verify("sha256", data, { key: publicKey, padding: 6, saltLength: against }, pss(salt)));
}
show("pss verify salt -5", () => verify("sha256", data, { key: publicKey, padding: 6, saltLength: -5 }, pss(10)));
show("node pss sha256", () => verify("sha256", data, { key: publicKey, padding: 6 }, Buffer.from(pssSha256Salt32, "hex")));
show("node pss sha256 salt 32", () => verify("sha256", data, { key: publicKey, padding: 6, saltLength: 32 }, Buffer.from(pssSha256Salt32, "hex")));
show("node pss sha256 salt 31", () => verify("sha256", data, { key: publicKey, padding: 6, saltLength: 31 }, Buffer.from(pssSha256Salt32, "hex")));
show("node pss sha1", () => verify("sha1", data, { key: publicKey, padding: 6 }, Buffer.from(pssSha1SaltMax, "hex")));
show("node pss sha512", () => verify("sha512", data, { key: publicKey, padding: 6, saltLength: 0 }, Buffer.from(pssSha512Salt0, "hex")));
show("node pss as pkcs1", () => verify("sha256", data, publicKey, Buffer.from(pssSha256Salt32, "hex")));
show("pss md5-sha1", () => verify("md5-sha1", data, { key: publicKey, padding: 6 }, sign("md5-sha1", data, { key: privateKey, padding: 6 })));
show("pss sha3-512", () => verify("sha3-512", data, { key: publicKey, padding: 6 }, sign("sha3-512", data, { key: privateKey, padding: 6 })));
show("pss sm3", () => verify("sm3", data, { key: publicKey, padding: 6 }, sign("sm3", data, { key: privateKey, padding: 6 })));

for (const padding of [0, 2, 3, 4, 7, -1]) show(`padding ${padding}`, () => sign("sha256", data, { key: privateKey, padding }).length);
show("verify padding 3", () => verify("sha256", data, { key: publicKey, padding: 3 }, sign("sha256", data, privateKey)));
show("padding 1.5", () => sign("sha256", data, { key: privateKey, padding: 1.5 }));
show("padding string", () => sign("sha256", data, { key: privateKey, padding: JSON.parse("\"6\"") }));
show("saltLength 1.5", () => sign("sha256", data, { key: privateKey, padding: 6, saltLength: 1.5 }));
show("salt without pss", () => sign("sha256", data, { key: privateKey, saltLength: 5 }).equals(sign("sha256", data, privateKey)));
show("dsaEncoding on rsa", () => sign("sha256", data, { key: privateKey, dsaEncoding: "ieee-p1363" }).length);
