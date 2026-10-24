#!/usr/bin/env python3
import struct, time, usb.core, usb.util

VID, PID = 0x1f3a, 0xefe8
FEL_WRITE = 0x101
FEL_EXEC  = 0x102
FEL_READ  = 0x103

class Fel:
    def __init__(self):
        dev = usb.core.find(idVendor=VID, idProduct=PID)
        if dev is None:
            raise RuntimeError("FEL device not found")
        try:
            if dev.is_kernel_driver_active(0):
                dev.detach_kernel_driver(0)
        except Exception:
            pass
        usb.util.claim_interface(dev, 0)
        cfg = dev.get_active_configuration()
        intf = cfg[(0, 0)]
        self.ep_out = usb.util.find_descriptor(
            intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
        self.ep_in = usb.util.find_descriptor(
            intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)
        if not self.ep_out or not self.ep_in:
            raise RuntimeError("endpoints not found")
        self.dev = dev

    def _awuc(self, reqtype, length):
        req = struct.pack('<8sIIHI10s', b'AWUC', length, 0x0c000000, reqtype, length, b'\0' * 10)
        self.dev.write(self.ep_out, req, timeout=5000)

    def _awus(self):
        b = bytes(self.dev.read(self.ep_in, 13, timeout=5000))
        if b[:4] != b'AWUS':
            raise RuntimeError("bad AWUS: %r" % b[:8])

    def _bulk_write(self, data):
        self._awuc(0x12, len(data))
        self.dev.write(self.ep_out, data, timeout=60000)
        self._awus()

    def _bulk_read(self, length):
        self._awuc(0x11, length)
        out = bytearray()
        while len(out) < length:
            chunk = self.dev.read(self.ep_in, length - len(out), timeout=60000)
            if not chunk:
                raise RuntimeError("short read")
            out += bytes(chunk)
        self._awus()
        return bytes(out)

    def _status(self):
        return self._bulk_read(8)

    def fel_write(self, addr, data):
        self._bulk_write(struct.pack('<IIII', FEL_WRITE, addr, len(data), 0))
        if data:
            self._bulk_write(bytes(data))
        return self._status()

    def fel_read(self, addr, length):
        self._bulk_write(struct.pack('<IIII', FEL_READ, addr, length, 0))
        data = self._bulk_read(length)
        self._status()
        return data

    def fel_exec(self, addr):
        self._bulk_write(struct.pack('<IIII', FEL_EXEC, addr, 0, 0))
        return self._status()

if __name__ == "__main__":
    f = Fel()
    t = time.time()
    d = f.fel_read(0x07090100, 4)
    print("GP0", d.hex(), "%.3fs" % (time.time() - t))
    c = f.fel_read(0x5B000, 512)
    print("buf", c[:16].hex())
