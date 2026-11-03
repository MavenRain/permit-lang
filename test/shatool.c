/* permit-lang S9 U1 (Q-S9-2 (a)): cairn-lang 60768e8 test/shatool.c, on
   host/sha256.c, with 3 pad rows (55, 56 and 64 bytes of "a"; each digest
   is `shasum -a 256` of the same bytes). 55 bytes: the pad and the length
   fit one block. 56 bytes: the length takes a second block. 64 bytes: the
   pad is a full second block. It prints `sha256 vectors: N checked`.

   cairn-lang text: host SHA-256 regressions (slice S4, O7): src/sha256.c against the
   FIPS 180-2 Appendix B examples B.1 to B.3 (the same values are in the
   NIST CSRC example file SHA256.pdf that FIPS 180-4 points to) and the
   empty string of the CAVP file SHA256ShortMsg.rsp (Len = 0, Q-S4-5). */
#include <stdio.h>
#include <string.h>

#include "sha256.h"

/* FIPS 180-2 B.3: 1,000,000 repetitions of "a". */
static unsigned char million[1000000];

/* The count of the rows that check ran. */
static int checked;

/* 1 when the digest of the SIZE bytes at DATA is not WANT (64 lowercase
   hex digits). */
static int check(const char *name, const unsigned char *data, size_t size, const char *want) {
  static const char digits[] = "0123456789abcdef";
  unsigned char digest[32];
  char hex[65];
  int i;
  checked++;
  sha256(data, size, digest);
  for (i = 0; i < 32; i++) {
    hex[2 * i] = digits[digest[i] >> 4];
    hex[2 * i + 1] = digits[digest[i] & 15u];
  }
  hex[64] = '\0';
  if (strcmp(hex, want) != 0)
    fprintf(stderr, "FAIL sha256 %s\n", name);
  return strcmp(hex, want) != 0;
}

int main(void) {
  const char *abc = "abc";
  const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  int fail = 0;
  memset(million, 'a', sizeof million);
  fail += check("empty string", (const unsigned char *)"", 0,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  fail += check("abc", (const unsigned char *)abc, strlen(abc),
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  fail += check("448-bit message", (const unsigned char *)two, strlen(two),
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  fail += check("one million a", million, sizeof million,
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  fail += check("55 bytes of a", million, 55,
                "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
  fail += check("56 bytes of a", million, 56,
                "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
  fail += check("64 bytes of a", million, 64,
                "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
  if (fail != 0)
    return 1;
  printf("sha256 vectors: %d checked\n", checked);
  return 0;
}
