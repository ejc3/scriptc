import {
  type KeyObject,
  type Sign,
  type Verify,
  createPrivateKey,
  createPublicKey,
  createSign,
  createVerify,
  sign,
  verify,
} from "node:crypto";

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

const rsa: KeyObject = createPrivateKey(rsaPkcs8);
const rsaPublic: KeyObject = createPublicKey(rsa);
const ec: KeyObject = createPrivateKey({ key: prime256v1, format: "pem", type: "sec1" });
const data = Buffer.from("typed data");
const pkcs1: Buffer = sign("sha256", data, rsa);
console.log(pkcs1.toString("base64").slice(0, 32), verify("sha256", data, rsaPublic, pkcs1));
const pss: Buffer = sign("sha256", data, { key: rsa, padding: 6, saltLength: 32 });
console.log(pss.length, verify("sha256", data, { key: rsaPublic, padding: 6 }, pss));
const signer: Sign = createSign("sha384");
signer.update("typed ").update(data);
const p1363: string = signer.sign({ key: ec, dsaEncoding: "ieee-p1363" }, "hex");
const verifier: Verify = createVerify("sha384");
verifier.update("typed ").update(data);
console.log(p1363.length, verifier.verify({ key: createPublicKey(ec), dsaEncoding: "ieee-p1363" }, p1363, "hex"));
const spki = rsaPublic.export({ format: "pem", type: "spki" }) as string;
const jwk = ec.export({ format: "jwk" });
console.log(spki.split("\n")[0], jwk.kty, jwk.crv, rsa.asymmetricKeyDetails?.modulusLength, ec.asymmetricKeyType);
