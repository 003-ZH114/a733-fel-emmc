#!/usr/bin/env python3
"""Dump eMMC sectors through the FEL payload using the xfel CLI.

Usage: dump.py <lba> <sectors> <outfile>
"""
import os, struct, subprocess, sys, tempfile, time

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_BIN = os.path.join(BASE, "04-payload", "binaries", "fel.bin")

CMD_ADDR = "0x5AFF0"
DATA_ADDR = "0x5B000"
CODE_ADDR = "0x57000"
CHUNK = 256          # sectors per exec
GP0 = "0x07090100"
GP1 = "0x07090104"
GP2 = "0x07090108"


def run(args, timeout=60):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout)


def read32(addr):
    r = run(["xfel", "read32", addr], 20)
    return r.stdout.strip()


def main():
    if len(sys.argv) != 4:
        print("usage: dump.py <lba> <sectors> <outfile>")
        sys.exit(2)
    lba = int(sys.argv[1], 0)
    total = int(sys.argv[2], 0)
    out = sys.argv[3]

    tmpdir = tempfile.mkdtemp(prefix="a733-dump-")
    cmd_file = os.path.join(tmpdir, "cmd.bin")
    chunk_file = os.path.join(tmpdir, "chunk.bin")

    run(["xfel", "write", CODE_ADDR, CODE_BIN], 30)

    done = 0
    t0 = time.time()
    with open(out, "wb") as f:
        while done < total:
            n = min(CHUNK, total - done)
            with open(cmd_file, "wb") as c:
                c.write(struct.pack("<IIII", 1, lba + done, n, 0))
            run(["xfel", "write", CMD_ADDR, cmd_file], 20)
            run(["xfel", "exec", CODE_ADDR], 60)
            st = read32(GP0)
            if st.lower() != "0x000000aa":
                print(f"\nERROR at lba {lba+done}: state={st} "
                      f"gp1={read32(GP1)} gp2={read32(GP2)}")
                sys.exit(1)
            r = run(["xfel", "read", DATA_ADDR, str(n * 512), chunk_file], 60)
            if r.returncode != 0:
                print("\nread failed:", r.stderr)
                sys.exit(1)
            with open(chunk_file, "rb") as c:
                f.write(c.read())
            done += n
            el = time.time() - t0
            rate = done * 512 / el / 1024
            if (done // n) % 16 == 0 or done >= total:
                sys.stdout.write(f"\r{lba+done} / {lba+total} sectors  "
                                 f"{done*512/1048576:.1f}MiB  {rate:.0f} KiB/s")
                sys.stdout.flush()
    print(f"\nOK -> {out}  ({os.path.getsize(out)} bytes, {time.time()-t0:.1f}s)")


if __name__ == "__main__":
    main()
