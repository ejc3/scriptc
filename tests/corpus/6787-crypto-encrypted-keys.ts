// Encrypted private keys: PKCS#8 with PBES2 (AES-256-CBC, AES-128-CBC,
// DES-EDE3-CBC) as PEM and DER, and traditional PEM with a DEK-Info
// header, read with string and Buffer passphrases; the missing and wrong
// passphrase errors; and encrypted keys passed to sign and createPublicKey.
import { createPrivateKey, createPublicKey, sign, verify } from "node:crypto";

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
const encPkcs8Aes256 = `-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIC5TBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQtWyTzXx+selglsD3
wRmY6AICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEEB3o90RKEiE4RLA6
mHjpY00EggKAONNHpk2HIBAFy920wqgW7Ijmc20JCY69+cxCgZstgNiBAD4sbHIs
UywSjkZiJPN6HZYSkv+zHRcOksmd0JQFWSXU6vIcs/DW4gW965A/YV7RUygPIIx8
hxvokJgnL/peLvAyle2gY7NtlYEL3vRTnMvxX7XxevDyu66470QXicvsH1s71rAr
ProAGsowy37ZrUIpaiH+wLJSa890jVtYWoq9u7zDhH6GqvcWUNExbSE+bAtYl5PC
098IAKiNLUQPc0SeA+xz10REyklRRJIStAr0ghPPGQ4LTJH+9eLqsbZt2BLXP5fF
4/b17VkCzkno1yDi4wpu3BiNKrpPA+bv9FLWwYETFJ+G+MdznTOdPCuDM1bwgZ+B
VQZKWgreWrldlgwtxo3UjEzBPR7sLze/76EqsVcpRJgepLT3YEv6XsDMGqgMJWcu
b1Yc8AEzG0sTCfTjm5HMkALf120gcCgIyThX/hgxIMYut7eyAYyyb1zyZvcOn/l4
rfNqTWU9qXttegWGjgm4qB9WCFpyI+kgSaFUNDtU5t60moi2XqsUyvYBlXHiG+mT
SXMUzM0qWUWV2gL4PXN6MAjO+YcKixdaCiBrw4H313mLH5k/AaZNRB3AX2zzSbqM
7YFxDfHJhbYDzSzYwSdOvePr9CdzCBxqdgk/5j1ikZZ3kcmSfu7cKRBZEEBln6Ub
vP+6wLg/jA7KsLZqjIADbCX7CwM3XpKGRGfwmEs0EtHj4DGp1rul7JEoVHjdZ/yQ
NPH1YLIa0JlNqpL5/o9gRAc+VnauPR+b3KSV3ViHxlX2tfGOjfPjyNxiH6PIBbrz
2y9sax3zz6R0KA4eNj/J14/R9zgHlW313w==
-----END ENCRYPTED PRIVATE KEY-----
`;
const encPkcs8Aes128 = `-----BEGIN ENCRYPTED PRIVATE KEY-----
MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBCMgNUyH1dd/hzQqHag
IlYqAgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBAgQQu3C2yz1obgUeI4Kb
hMryXQSBkLlgd18Bkx9Leu+b0Ci8l8HvCj4d4PcnL7fKa1MipsYw/eYOBnF5iYFU
M9tismyS28jKuqGNX66o8FLjnu4wWAXY32Oi3cuxC9sguAU8QriGdha/bhAIEa2D
v2obBKMMGFzbw1UiUjGV4BbUSZY53AFIu3akXuTEEKGIuv7fk8vRNe6La2jRYONc
ESj6Sbe/0g==
-----END ENCRYPTED PRIVATE KEY-----
`;
const encPkcs8Des3 = `-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIC3DBWBgkqhkiG9w0BBQ0wSTAxBgkqhkiG9w0BBQwwJAQQLR8j5oJ/B1hchTZH
Z2AsBQICCAAwDAYIKoZIhvcNAgkFADAUBggqhkiG9w0DBwQIvrItUqM5crIEggKA
qdvcAoRULYNPA7gIwWSOlOWzfTsTJC2qb/gvcukzhf15dMRDel3J0rQqPub5s7k8
fllNYrS93t1YZsBlOBIoZo/hVnbNDprdaeJfaPHDgJnADsDz+XknXdDsd+uiNt9c
YCthqf7ldX3/XjOmSYhPe++PCRdcAQKWdWhpLYMaTlUfWVX98uWO3eEo1MuWBBUU
IFkh0r0HAUEewTLV/xLU96yOOGg+//wdPmQbfDyuz0MVp76ufQDl4ulk9Wtt8iWC
86KIkeDdGBH4I2Nj4VxMSfOhp4uo4sEYfIb5qDQTeBNID264JvryWXz0ZQFt18gL
BMXyfDlnhqQLdJTqnGZW8uJd1y5uWgurTr6rK8w5eusnjXbP2NhqAlkkdrM7n5k2
zcjyl3szXA5dLEWnH+flf3yoH+xUleNxHIG0V0j5ItSp/aUE3lUmiGuyA2fW0kD6
lDWvFwXo1n5/itc/ULTbNVBJ5L3+p0u6XTm85CLrurl2fIe9NN+FqiXixwaEkMU/
f3lBXbjn7UyGW5oGMPcfOiaB5sL9gvYeuQrR9PCt7K//YzNnkwMY+35yW7mqtqRA
EYqJrvfRllXUr6K4eKvvUXzO4Y/Ke1JMT59Cy6C5x+Tbq7L8cLyxw/w7Caq5J9WA
vYmmOb8XCVqtDAEfkkaRl/T/vIJ2R1tHwbzGHXORur4C/hRClJlc7g1dvkSKJLWj
WqHuCuUHNeufRmYBCsc4/LF67DcRBbcehYlbMjTMmRCj8TEzbA6FgvPyPszdzpTY
nwslWf3r3iA0stgRMx/g+3MOPpnYD83rV5TpET87Bhtkk0CchRsd/PitMLWw+dhz
kMIVh6eBB/k9iwja3E8Jcw==
-----END ENCRYPTED PRIVATE KEY-----
`;
const encPkcs1Aes256 = `-----BEGIN RSA PRIVATE KEY-----
Proc-Type: 4,ENCRYPTED
DEK-Info: AES-256-CBC,85F92BDBB98EE141A329936B614FD6B4

9n7KnZyz3fxnT0t4rDYPTkzrrxivnZO0ygacjVMDC2Sj50ApqtyVt/tjcJU+H9g+
c/XGVpxNQ65956wHg06DmNSzej4R6oSCftkGEwcEBWtW903I6okE+zUrBQ+6RbsQ
BcRkXjs/wQl42iqGNU/lHOuYIEfzNKmY7ntKGH3Y5n1cfQ+SoXqupaj7psNg8SDu
77sJf1QacIrcjozcIPwYUldTVuZkROFqfctFd8k7fbc0eKjtyIAgBI1tvHgs60gm
7+vOvAIkP7buxrWqh795yyinW1po0UacrL/cjxL+8I40JpOvP8msh/JAvtWRYFLx
TAwn3rY4mNu6cLl+Jcap4C1uXDYY9Jd7rkPgQ+nWtz83mRHNGJHDnpFGvBg1w388
X1G9FOU6ZA4OyZKTJseUZBAOWpgiq+4ZGXH7avs35NSZpi8kZK1jsAgVvwyFCkH+
ANcEhFsORJ0oqHBABARE/vTws5aVEe7d5mpU+58hfnCv/mPL2C3uQU+/X+lzG8wC
QH376R1jbqWvy4EZ8ygrYZSLW/vY2jcXgO+aGwyJq1tmLd+QYZp34ZmxZaKuWnPu
qRAAjhTCYoLguy3YEAN+xynuxvuxVmWSsc4RWVoSmpve5S2TjCTCodiR/ZNtDeAn
516JU/erMXLuaqmieQO4LiU9R0sjQScndqmXTjII1FLRa/+S6aplGPaVAie1AAGs
nljuBL/Fg824yAPNhkS5mPv8pH935je9oBt3/zOLFdpy44kEdrdgTEMyP20gSQV+
YkDoCz2YM6/13wumhqvBMNbBJ2Z/stDW7bTYzYCS0tKrQyJTHnYpE6UESJHfINAq
-----END RSA PRIVATE KEY-----
`;
const encSec1Aes128 = `-----BEGIN EC PRIVATE KEY-----
Proc-Type: 4,ENCRYPTED
DEK-Info: AES-128-CBC,5C17C7DF4720C9F98201377A355E5FCF

QCnvYSZeCxvqJD+2cBhbnzZPiuzxHvdSyPBU9uOG8ww2qe5MA/Qf5JMsp6l9Bf4s
uEYqkomzuJkXEcpVraC23GqQq4ayFeZndC0bv9eR+k9K5QtiPLQXRizK1S/EBfKH
gK5zrWV7AmUnRRYwu6T2ahv7etJqVSEyll784Y4/SN8=
-----END EC PRIVATE KEY-----
`;
const encPkcs8DerBase64 = `MIIC5TBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQLJGMZ1fJiqjPyHZF0U6DBwICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEENzEAgc/W5qSy+BCUCPJVWUEggKAlTk2eIQF8RtmDlVcB9an0Ja8/ukqG3OwS83XXi0AmJ2GS3pM06AyKwU+zlQW1OaWgEeTwYUzM/i+P4Bf9rCYqVgquiz55lVYkCduoXCctgoxB9jKJ9ffItSGKrRd4cp5L/RGZHEpuLos/X0ktQxhPZlmXdNdyNBAqbfeT014a2uKHfZEmc21EjnqjWX5RWoKEHMktPsf6WIxyP1f3OYMn9cCNnKpa6FcnF26ou62NIRfiH/J2bXNQdvGHRJx0iy9hINlllw4nMJvnRPD47Q88KG20PKKIhKKRbC8kntdge3Lvu41ncspPp+2bhQCnUuHoAK1r0FMCfckIgWfN6Qj/W0l+VTK2GLgrijN/lsskk5fBrkaT3CrYupbcXs1zG4RdiAVPQP/CEuuSLMI+QcKuyWebgDVY8aMBgsAWSC5I7tgu0VuxeZ6A3XTLrTd/9ozif/JpfN+3Z9jzD2V6GpTYq/wa8RgOh6qUYMrKhyAbSwK1hARArVuuQBUmpuEKOoImHBsZWjgFZw1EKAXqzsa01hd8f4HTHoOGqaVAHYHwVQmPUeynN5tfoScVBKwjLv10aOI/Ot4ZGNGiGWHoCZDnwDKFWvDkcI3Wf9YYmQB9J2qyKsKKZdCkoL1SgcLGWoyPfDYHCep1e8EigOb9rW9is+epPmO/9JV8VWhupX2XpIlKHiMUUDtEt+q8TthLAUNB94TYJjiey5XuBW2f8ncO70Gsu5Qwx1P8OLNJ9lW5ISeQr9SOP1IySCBmWNw3fnE4CAiJr+tfxdTds9h2RghtY/zVPhIlJeyjZNG4Zszdrh3YxWdpohP/ENIZ5eQNx+Bheq2Oa4e39nq3CL/rq1l4w==`;

function show(label: string, fn: () => unknown): void {
  try {
    const value = fn();
    console.log(label, Buffer.isBuffer(value) ? `Buffer(${value.length})` : JSON.stringify(value));
  } catch (e) {
    const error = e as Error & { code?: string; opensslErrorStack?: string[] };
    console.log(label, "throws", error.name, error.code, error.message, JSON.stringify(error.opensslErrorStack ?? null));
  }
}

const rsa = createPrivateKey(rsaPkcs8);
const ec = createPrivateKey(prime256v1);
const encrypted: [string, string, boolean][] = [
  ["pkcs8 aes-256-cbc", encPkcs8Aes256, true],
  ["pkcs8 aes-128-cbc", encPkcs8Aes128, false],
  ["pkcs8 des-ede3-cbc", encPkcs8Des3, true],
  ["pkcs1 aes-256-cbc", encPkcs1Aes256, true],
  ["sec1 aes-128-cbc", encSec1Aes128, false],
];
for (const [label, pem, isRsa] of encrypted) {
  const expected = isRsa ? rsa : ec;
  console.log(label, pem.split("\n")[0], pem.includes("DEK-Info") ? pem.split("\n")[2].split(",")[0] : "");
  show(" string passphrase", () => createPrivateKey({ key: pem, passphrase: "top secret" }).equals(expected));
  show(" buffer passphrase", () => createPrivateKey({ key: pem, passphrase: Buffer.from("top secret") }).equals(expected));
  show(" encoded passphrase", () => createPrivateKey({ key: pem, passphrase: "746f7020736563726574", encoding: "hex" }).type);
  show(" no passphrase", () => createPrivateKey(pem));
  show(" wrong passphrase", () => createPrivateKey({ key: pem, passphrase: "wrong" }));
  show(" public", () => createPublicKey({ key: pem, passphrase: "top secret" } as never).equals(createPublicKey(expected)));
  show(" public without passphrase", () => createPublicKey(pem));
  show(" sign", () => verify("sha256", Buffer.from("x"), createPublicKey(expected), sign("sha256", Buffer.from("x"), { key: pem, passphrase: "top secret" })));
  show(" sign without passphrase", () => sign("sha256", Buffer.from("x"), pem));
}
const der = Buffer.from(encPkcs8DerBase64, "base64");
show("der", () => createPrivateKey({ key: der, format: "der", type: "pkcs8", passphrase: "top secret" }).equals(rsa));
show("der base64 string", () => createPrivateKey({ key: encPkcs8DerBase64, format: "der", type: "pkcs8", encoding: "base64", passphrase: "top secret" }).type);
show("der no passphrase", () => createPrivateKey({ key: der, format: "der", type: "pkcs8" }));
show("der wrong passphrase", () => createPrivateKey({ key: der, format: "der", type: "pkcs8", passphrase: "nope" }));
show("der as pkcs1", () => createPrivateKey({ key: der, format: "der", type: "pkcs1", passphrase: "top secret" }));
