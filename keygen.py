#!/usr/bin/env python3
# Makes Frutiger Nano activation keys. Needs src/secret.h (keep it private).
import hashlib, re, secrets, sys, os
A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
here = os.path.dirname(os.path.abspath(__file__))
secret = re.search(r'NANO_SECRET\s+"([^"]+)"', open(os.path.join(here, "src", "secret.h")).read()).group(1)
def key():
    body = "".join(secrets.choice(A) for _ in range(8))
    h = hashlib.sha256((secret + body).encode()).digest()
    chk = "".join(A[b & 31] for b in h[:4])
    return f"NANO-{body[:4]}-{body[4:]}-{chk}"
for _ in range(int(sys.argv[1]) if len(sys.argv) > 1 else 5):
    print(key())
