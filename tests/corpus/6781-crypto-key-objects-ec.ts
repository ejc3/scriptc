// Public and private EC KeyObjects on the curves the runtime implements:
// sec1, pkcs8 and spki PEM and DER, JWK on the four JWK curves and its
// unsupported-curve error, raw-public (compressed and uncompressed) and
// raw-private export, and the encoding errors that depend on the key type.
import { createPrivateKey, createPublicKey, type KeyObject } from "node:crypto";

const prime256v1 = `-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIG47D/R5x59oozeFFKzw2l2Q8iiLfTOWvJvsvgg+dD5voAoGCCqGSM49
AwEHoUQDQgAEWy178rqIN9Zuud1QA7W7OFQ8sj7+RF6Vm7jxf3YGdc5yC2sH/nyu
nOob6jMkc73PRvSGwbEW5p5cVz1zt7q6Rw==
-----END EC PRIVATE KEY-----
`;
const secp384r1 = `-----BEGIN EC PRIVATE KEY-----
MIGkAgEBBDBrSjmM/W5fHqcZSv6B8bl+YqmhlS8Vjsn8n1oBMq+vlXS3uSTwqmxA
Yyj5rJDykZugBwYFK4EEACKhZANiAASa1gyYbPm96cgFCFB0LotZu+ynoYoulxaY
PC+6dRIPufyZrEbV7M6xuR6InIhE1uS9zU2D8/Qi4sjKkzK311wkHkkPm4k6Wr24
UY/a05wpxcC0hYm/IDwmwM2e/19gsv4=
-----END EC PRIVATE KEY-----
`;
const secp521r1 = `-----BEGIN EC PRIVATE KEY-----
MIHcAgEBBEIBxjG6lHS7q5r8GrKAtWhIQIg8M3QGJPaMk4UeG+oimFjPSa8TSrmY
x2JHZQxOE4h1sWpBgauKV6w3TBAitLu3Fe6gBwYFK4EEACOhgYkDgYYABABiA+UI
NVUpphrPbklUiSftyNd2sfTWK7NDag4CMhtOJzPxGbmXMCb+kIr+j0Upixk5gibQ
LZxscJQsvoayMNyKLAFDTVC6wEge3tHhueZOhwT9aAmWAY3JDmvEEauRQRlh3bb4
/gZE0i43hedzZLw6VHoSICgJLVuXhF0VMnoO1cS7gQ==
-----END EC PRIVATE KEY-----
`;
const secp256k1 = `-----BEGIN EC PRIVATE KEY-----
MHQCAQEEIGp4xrs22hS1D/KDwqkuCkdMhyTbNJISQineqE4DOl21oAcGBSuBBAAK
oUQDQgAEReKZzpb3TZKAfo80bQuH1fLZvZrONsEOzBJKKbUi9F6k2y8yAiJ01mzW
GGIRBVmjmRdW81p8IBU6KiWQp3hZWQ==
-----END EC PRIVATE KEY-----
`;
const secp224r1 = `-----BEGIN EC PRIVATE KEY-----
MGgCAQEEHCjC9t9M0cG4CT76K3Q3/bHHak/NutAy+JsyHSCgBwYFK4EEACGhPAM6
AAR5ckATN2I/FMJjRDPCTaXf1kTq1xzjpFsGHEAutOIVByH2mXe1p6Dqz9+j8W2p
3P5NR1JEJEGg4Q==
-----END EC PRIVATE KEY-----
`;
const brainpoolP256r1 = `-----BEGIN EC PRIVATE KEY-----
MHgCAQEEIEBDbaetrMqtOnf+cQqraEUnzgjCGqkvpRV9j2fXQiXjoAsGCSskAwMC
CAEBB6FEA0IABILOVkCEBxxWwHrvWB8ZKEJ9eYo/UAINzLAMfmVhLD+PJ4byqMhC
CDuc3ipqTx3Mt2IpkUd/jmGzZGobvbUPEec=
-----END EC PRIVATE KEY-----
`;

function show(label: string, fn: () => unknown): void {
  try {
    const value = fn();
    console.log(label, typeof value === "string" ? JSON.stringify(value) : Buffer.isBuffer(value) ? value.toString("hex") : JSON.stringify(value));
  } catch (e) {
    const error = e as Error & { code?: string };
    console.log(label, "throws", error.name, error.code, error.message);
  }
}

const curves: [string, string][] = [
  ["prime256v1", prime256v1],
  ["secp384r1", secp384r1],
  ["secp521r1", secp521r1],
  ["secp256k1", secp256k1],
  ["secp224r1", secp224r1],
  ["brainpoolP256r1", brainpoolP256r1],
];
for (const [name, pem] of curves) {
  const privateKey = createPrivateKey(pem);
  const publicKey = createPublicKey(privateKey);
  console.log(name, privateKey.type, privateKey.asymmetricKeyType, privateKey.asymmetricKeyDetails?.namedCurve);
  const sec1 = privateKey.export({ format: "pem", type: "sec1" }) as string;
  const pkcs8 = privateKey.export({ format: "pem", type: "pkcs8" }) as string;
  const spki = publicKey.export({ format: "pem", type: "spki" }) as string;
  console.log(sec1 === pem);
  console.log(pkcs8);
  console.log(spki);
  const pkcs8Der = privateKey.export({ format: "der", type: "pkcs8" });
  const sec1Der = privateKey.export({ format: "der", type: "sec1" });
  const spkiDer = publicKey.export({ format: "der", type: "spki" });
  console.log(pkcs8Der.toString("hex"));
  console.log(sec1Der.length, spkiDer.toString("hex"));
  const again: KeyObject[] = [
    createPrivateKey(pkcs8),
    createPrivateKey({ key: pkcs8Der, format: "der", type: "pkcs8" }),
    createPrivateKey({ key: sec1Der, format: "der", type: "sec1" }),
  ];
  console.log(again.map((key) => key.equals(privateKey)).join());
  const publics: KeyObject[] = [
    createPublicKey(spki),
    createPublicKey({ key: spkiDer, format: "der", type: "spki" }),
    createPublicKey(pem),
    createPublicKey({ key: sec1Der, format: "der", type: "sec1" } as never),
  ];
  console.log(publics.map((key) => key.type + key.equals(publicKey)).join());
  show("jwk", () => privateKey.export({ format: "jwk" }));
  show("public jwk", () => publicKey.export({ format: "jwk" }));
  show("raw-public", () => publicKey.export({ format: "raw-public" } as never));
  show("raw-public compressed", () => publicKey.export({ format: "raw-public", type: "compressed" } as never));
  show("raw-private", () => privateKey.export({ format: "raw-private" } as never));
  show("pkcs1", () => privateKey.export({ format: "pem", type: "pkcs1" }));
  show("public pkcs1", () => publicKey.export({ format: "der", type: "pkcs1" }));
}

const p256 = createPrivateKey(prime256v1);
const jwk = p256.export({ format: "jwk" });
show("from jwk", () => createPrivateKey({ key: jwk, format: "jwk" }).equals(p256));
show("public from jwk", () => createPublicKey({ key: { kty: "EC", crv: jwk.crv, x: jwk.x, y: jwk.y }, format: "jwk" }).asymmetricKeyDetails);
show("bad crv", () => createPrivateKey({ key: { kty: "EC", crv: "P-999", x: jwk.x, y: jwk.y, d: jwk.d }, format: "jwk" }));
show("short x", () => createPublicKey({ key: { kty: "EC", crv: "P-256", x: "AAAA", y: jwk.y }, format: "jwk" }));
show("no y", () => createPublicKey({ key: { kty: "EC", crv: "P-256", x: jwk.x }, format: "jwk" }));
show("private from public jwk", () => createPrivateKey({ key: { kty: "EC", crv: jwk.crv, x: jwk.x, y: jwk.y }, format: "jwk" }));
show("raw-public bad type", () => createPublicKey(p256).export({ format: "raw-public", type: "hybrid" } as never));
show("raw-private public", () => createPublicKey(p256).export({ format: "raw-private" } as never));
