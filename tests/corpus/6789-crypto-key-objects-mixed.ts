// Public and private KeyObjects next to the rest of node:crypto: typed
// helpers and collections of keys, util.types.isKeyObject and instanceof
// across key types, secret-key consumers given an asymmetric key, and
// signatures in a typed signing service.
import {
  createCipheriv,
  createHmac,
  createPrivateKey,
  createPublicKey,
  createSecretKey,
  createSign,
  createVerify,
  hkdfSync,
  KeyObject,
  sign,
  verify,
} from "node:crypto";
import { types } from "node:util";

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

function show(label: string, fn: () => unknown): void {
  try {
    console.log(label, JSON.stringify(fn()));
  } catch (e) {
    const error = e as Error & { code?: string };
    console.log(label, "throws", error.name, error.code, error.message);
  }
}

interface KeyPair {
  privateKey: KeyObject;
  publicKey: KeyObject;
}

function pairOf(pem: string): KeyPair {
  const privateKey = createPrivateKey(pem);
  return { privateKey, publicKey: createPublicKey(privateKey) };
}

class Signer {
  private readonly pairs = new Map<string, KeyPair>();
  add(name: string, pem: string): void {
    this.pairs.set(name, pairOf(pem));
  }
  sign(name: string, message: string): string {
    const pair = this.pairs.get(name)!;
    const signer = createSign("sha256");
    signer.update(message);
    return signer.sign(pair.privateKey, "base64");
  }
  verify(name: string, message: string, signature: string): boolean {
    const verifier = createVerify("sha256");
    verifier.update(message);
    return verifier.verify(this.pairs.get(name)!.publicKey, signature, "base64");
  }
  describe(): string[] {
    const out: string[] = [];
    for (const [name, pair] of this.pairs) {
      const details = pair.publicKey.asymmetricKeyDetails;
      out.push(`${name}: ${pair.privateKey.asymmetricKeyType} ${details?.modulusLength ?? details?.namedCurve}`);
    }
    return out;
  }
}

const service = new Signer();
service.add("rsa", rsaPkcs8);
service.add("ec", prime256v1);
console.log(service.describe().join("; "));
for (const name of ["rsa", "ec"]) {
  const signature = service.sign(name, "hello");
  console.log(name, service.verify(name, "hello", signature), service.verify(name, "goodbye", signature));
}

const rsa = pairOf(rsaPkcs8);
const ec = pairOf(prime256v1);
const secret = createSecretKey(Buffer.from("0123456789abcdef"));
const keys: KeyObject[] = [rsa.privateKey, rsa.publicKey, ec.privateKey, ec.publicKey, secret];
for (const key of keys) {
  console.log(key.type, key.asymmetricKeyType, key.symmetricKeySize, key instanceof KeyObject, types.isKeyObject(key));
}
console.log(types.isKeyObject(rsaPkcs8), types.isKeyObject({}), keys.filter((key) => key.type === "public").length);
const byType = new Map<string, KeyObject[]>();
for (const key of keys) byType.set(key.type, [...(byType.get(key.type) ?? []), key]);
console.log([...byType.keys()].join(), byType.get("private")!.map((key) => key.asymmetricKeyType).join());

show("hmac with private key", () => createHmac("sha256", rsa.privateKey).digest("hex"));
show("hmac with public key", () => createHmac("sha256", ec.publicKey).digest("hex"));
show("cipher with public key", () => createCipheriv("aes-128-cbc", rsa.publicKey, Buffer.alloc(16)));
show("hkdf with private key", () => hkdfSync("sha256", ec.privateKey, "", "", 16));
show("secret equals private", () => secret.equals(rsa.privateKey));
show("private equals secret", () => rsa.privateKey.equals(secret));
show("sign with secret", () => sign("sha256", Buffer.from("x"), secret));
show("verify with secret", () => verify("sha256", Buffer.from("x"), secret, Buffer.alloc(4)));
show("public of secret", () => createPublicKey(secret));
show("cross-type verify", () => verify("sha256", Buffer.from("x"), ec.publicKey, sign("sha256", Buffer.from("x"), rsa.privateKey)));
