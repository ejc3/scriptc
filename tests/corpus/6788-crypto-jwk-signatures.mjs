// JWK keys with signatures: sign and verify with JWK key input and the
// signing options beside it, createSign and createVerify with JWK keys,
// JWKs through JSON, and the public half of a private JWK.
import { createPrivateKey, createPublicKey, createSign, createVerify, sign, verify } from "node:crypto";

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

function show(label, fn) {
  try {
    const value = fn();
    console.log(label, JSON.stringify(value));
  } catch (e) {
    console.log(label, "throws", e.name, e.code, e.message);
  }
}

const data = Buffer.from("jwk data");
for (const [name, pem] of [["rsa", rsaPkcs8], ["p256", prime256v1], ["p384", secp384r1], ["p521", secp521r1], ["k1", secp256k1]]) {
  const jwk = JSON.parse(JSON.stringify(createPrivateKey(pem).export({ format: "jwk" })));
  const publicJwk = { ...jwk };
  for (const field of ["d", "p", "q", "dp", "dq", "qi"]) delete publicJwk[field];
  show(`${name} keys`, () => Object.keys(jwk));
  show(`${name} sign jwk`, () => {
    const signature = sign("sha256", data, { key: jwk, format: "jwk" });
    return [verify("sha256", data, { key: publicJwk, format: "jwk" }, signature), verify("sha256", data, { key: jwk, format: "jwk" }, signature)];
  });
  show(`${name} options beside jwk`, () => {
    const options = name === "rsa" ? { padding: 6, saltLength: 16 } : { dsaEncoding: "ieee-p1363" };
    const signature = sign("sha512", data, { key: jwk, format: "jwk", ...options });
    return [signature.length, verify("sha512", data, { key: publicJwk, format: "jwk", ...options }, signature)];
  });
  show(`${name} sign object`, () => {
    const signature = createSign("sha384").update(data).sign({ key: jwk, format: "jwk" }, "base64");
    return createVerify("sha384").update(data).verify({ key: publicJwk, format: "jwk" }, signature, "base64");
  });
  show(`${name} public of private jwk`, () => createPublicKey({ key: jwk, format: "jwk" }).export({ format: "jwk" }));
  show(`${name} private from public jwk`, () => createPrivateKey({ key: publicJwk, format: "jwk" }));
  show(`${name} sign with public jwk`, () => sign("sha256", data, { key: publicJwk, format: "jwk" }));
}
show("jwk number", () => sign("sha256", data, { key: 5, format: "jwk" }));
show("jwk null", () => createPublicKey({ key: null, format: "jwk" }));
show("jwk array", () => createPublicKey({ key: [], format: "jwk" }));
show("jwk kty number", () => createPublicKey({ key: { kty: 5 }, format: "jwk" }));
show("jwk crv number", () => createPublicKey({ key: { kty: "EC", crv: 5, x: "AA", y: "AA" }, format: "jwk" }));
