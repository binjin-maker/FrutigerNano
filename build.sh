#!/usr/bin/env bash
# Cross-compile FrutigerNano.exe (needs: pip install ziglang)
set -e
cd "$(dirname "$0")/src"
python3 -m ziglang cc -target x86_64-windows-gnu -O2 -s -o ../FrutigerNano.exe FrutigerNano.c app.rc \
  -Wl,--subsystem,windows -luser32 -lgdi32 -lcomctl32 -ldwmapi -luxtheme -lwininet -lbcrypt -lshell32
echo "built FrutigerNano.exe"
