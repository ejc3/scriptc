// ECDSA signatures: crypto.sign and crypto.verify over the implemented
// curves with DER and IEEE P1363 encodings, verification of signatures
// Node made, the DER strictness OpenSSL applies, and the digests the EC
// provider refuses.
import { createPrivateKey, createPublicKey, sign, verify } from "node:crypto";

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

const data = Buffer.from("signed by Node");
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
  for (const digest of ["sha1", "sha256", "sha512", "sha3-256", "md5", "ripemd160", "blake2b512", "keccak-256", "md5-sha1", "sha512-224"]) {
    show(`${name} ${digest}`, () => {
      const der = sign(digest, data, privateKey);
      const p1363 = sign(digest, data, { key: privateKey, dsaEncoding: "ieee-p1363" });
      return [
        der[0], p1363.length,
        verify(digest, data, publicKey, der),
        verify(digest, data, { key: publicKey, dsaEncoding: "ieee-p1363" }, p1363),
        verify(digest, data, { key: publicKey, dsaEncoding: "ieee-p1363" }, der),
        verify(digest, data, publicKey, p1363),
        verify(digest, Buffer.from("other"), publicKey, der),
        verify(digest, data, pem, der),
      ];
    });
  }
}

const p256 = createPublicKey(prime256v1);
show("node p256", () => verify("sha256", data, p256, Buffer.from(p256Der, "hex")));
show("node p256 p1363", () => verify("sha384", data, { key: p256, dsaEncoding: "ieee-p1363" }, Buffer.from(p256P1363, "hex")));
show("node p256 wrong digest", () => verify("sha1", data, p256, Buffer.from(p256Der, "hex")));
show("node k1", () => verify("sha256", data, createPublicKey(secp256k1), Buffer.from(k1Der, "hex")));
show("node p521", () => verify("sha512", data, createPublicKey(secp521r1), Buffer.from(p521Der, "hex")));
show("null algorithm", () => verify(null, data, p256, Buffer.from(p256Der, "hex")));
const der = Buffer.from(p256Der, "hex");
show("trailing byte", () => verify("sha256", data, p256, Buffer.concat([der, Buffer.from([0])])));
show("long-form length", () => {
  const body = der.subarray(2);
  return verify("sha256", data, p256, Buffer.concat([Buffer.from([0x30, 0x81, body.length]), body]));
});
show("empty", () => verify("sha256", data, p256, Buffer.alloc(0)));
show("short p1363", () => verify("sha384", data, { key: p256, dsaEncoding: "ieee-p1363" }, Buffer.from(p256P1363, "hex").subarray(1)));
show("garbage", () => verify("sha256", data, p256, Buffer.from("xx")));
show("shake256", () => sign("shake256", data, createPrivateKey(prime256v1)));
show("verify shake128", () => verify("shake128", data, p256, der));
show("kmac", () => sign("keccak-kmac128", data, createPrivateKey(prime256v1)));
show("bad dsaEncoding", () => sign("sha256", data, { key: createPrivateKey(prime256v1), dsaEncoding: "raw" as "der" }));
show("padding on ec", () => sign("sha256", data, { key: createPrivateKey(prime256v1), padding: 6 })[0]);
