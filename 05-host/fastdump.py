#!/usr/bin/env python3
"""Dump eMMC sectors through the persistent pyusb FEL client.

Usage: fastdump.py <start_lba> <sectors> <outfile> [--append] [--nocode]
"""
import os, struct, sys, time
from felclient import Fel

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_BIN = os.path.join(BASE, "04-payload", "binaries", "fel.bin")

CMD = 0x5AFF0
BUF = 0x5B000
CODE = 0x57000
CHUNK = 512


def main():
    if len(sys.argv) < 4:
        print("usage: fastdump.py <start_lba> <sectors> <outfile> [--append] [--nocode]")
        sys.exit(2)
    lba = int(sys.argv[1], 0)
    total = int(sys.argv[2], 0)
    out = sys.argv[3]
    append = "--append" in sys.argv[4:]

    f = Fel()
    if "--nocode" not in sys.argv[4:]:
        with open(CODE_BIN, "rb") as fh:
            f.fel_write(CODE, fh.read())

    mode = "ab" if append else "wb"
    done = 0
    t0 = time.time()
    with open(out, mode) as fo:
        while done < total:
            n = min(CHUNK, total - done)
            cmd = struct.pack("<IIII", 1, lba + done, n, 0)
            f.fel_write(CMD, cmd)
            f.fel_exec(CODE)
            st = f.fel_read(0x07090100, 4)
            state = struct.unpack("<I", st)[0]
            if state != 0xAA:
                gp1 = struct.unpack("<I", f.fel_read(0x07090104, 4))[0]
                gp2 = struct.unpack("<I", f.fel_read(0x07090108, 4))[0]
                print(f"\nERROR state=0x{state:x} gp1=0x{gp1:x} gp2=0x{gp2:x} "
                      f"at lba {lba+done}")
                sys.exit(1)
            fo.write(f.fel_read(BUF, n * 512))
            done += n
            if (done // n) % 8 == 0 or done >= total:
                el = time.time() - t0
                rate = done * 512 / el / 1024
                print(f"\r{lba+done} / {lba+total}  {done*512/1048576:.1f}MiB  "
                      f"{rate:.0f} KiB/s", end="", flush=True)
    print(f"\nOK {out} +{done*512} bytes in {time.time()-t0:.1f}s")


if __name__ == "__main__":
    main()
