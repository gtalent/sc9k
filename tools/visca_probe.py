#!/usr/bin/env python3
"""Measures VISCA PTZ behaviour of a camera: zoom scale, landing accuracy, max speeds, speed steps."""
import socket
import sys
import time

HOST = sys.argv[1]
PORT = int(sys.argv[2])
ZOOM_SCALE = float(sys.argv[3])  # current learned scale, used only to restore the start zoom

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(("0.0.0.0", 52381))
except OSError:
    sock.bind(("0.0.0.0", 0))
sock.settimeout(0.3)
seq = 1


def send(payload, inquiry=False):
    global seq
    hdr = bytes([0x01, 0x10 if inquiry else 0x00, 0, len(payload)]) + seq.to_bytes(4, "big")
    seq += 1
    sock.sendto(hdr + bytes(payload), (HOST, PORT))


def drain():
    sock.settimeout(0.0)
    try:
        while True:
            sock.recvfrom(1024)
    except (BlockingIOError, socket.timeout):
        pass
    finally:
        sock.settimeout(0.3)


def inquire(payload, sizes, tries=3):
    """Returns the data bytes of an inquiry reply whose data length is in sizes, or None."""
    for _ in range(tries):
        drain()
        send(payload, True)
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            try:
                data, _ = sock.recvfrom(1024)
            except socket.timeout:
                break
            msg = data[8:] if len(data) >= 8 and data[0] == 0x01 else data
            if len(msg) >= 3 and msg[1] == 0x50 and msg[-1] == 0xFF and len(msg) - 3 in sizes:
                return msg[2:-1]
            if len(msg) >= 3 and (msg[1] & 0xF0) == 0x60:
                return ("error", msg.hex(" "))
    return None


def nib(b):
    v = 0
    for x in b:
        v = (v << 4) | (x & 0x0F)
    return v


def signed(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


pan_nibbles = 4


def pt():
    global pan_nibbles
    d = inquire([0x81, 0x09, 0x06, 0x12, 0xFF], (8, 9))
    if not d or isinstance(d, tuple):
        return None
    pan_nibbles = len(d) - 4
    return signed(nib(d[:pan_nibbles]), pan_nibbles * 4), signed(nib(d[pan_nibbles:]), 16)


def zoom():
    d = inquire([0x81, 0x09, 0x04, 0x47, 0xFF], (4,))
    if not d or isinstance(d, tuple):
        return None
    return nib(d)


def nibbles(v, n):
    v &= (1 << (n * 4)) - 1
    return [(v >> (4 * (n - 1 - i))) & 0x0F for i in range(n)]


def zoom_direct(v):
    send([0x81, 0x01, 0x04, 0x47] + nibbles(v, 4) + [0xFF])


def pt_abs(pan, tilt, ps=12, ts=12):
    send([0x81, 0x01, 0x06, 0x02, ps, ts] + nibbles(pan, pan_nibbles) + nibbles(tilt, 4) + [0xFF])


def pt_drive(ps, ts, h, v):
    # h: 1 left, 2 right, 3 stop; v: 1 up, 2 down, 3 stop
    send([0x81, 0x01, 0x06, 0x01, ps, ts, h, v, 0xFF])


def settle(read, timeout=20.0, still=0.6):
    last, since = None, time.monotonic()
    end = time.monotonic() + timeout
    time.sleep(0.3)
    while time.monotonic() < end:
        cur = read()
        if cur is not None and cur == last:
            if time.monotonic() - since >= still:
                return cur
        else:
            last, since = cur, time.monotonic()
        time.sleep(0.05)
    return last


print(f"camera {HOST}:{PORT}")
start_pt, start_zoom = pt(), zoom()
print(f"start: pan/tilt {start_pt} ({pan_nibbles} nibble pan), zoom {start_zoom}")
if start_pt is None or start_zoom is None:
    sys.exit("no position replies, aborting")

print("\n== identity / max speed ==")
v = inquire([0x81, 0x09, 0x00, 0x02, 0xFF], (7, 8, 9, 10))
print("version inq:", v.hex(" ") if isinstance(v, (bytes, bytearray)) else v)
v = inquire([0x81, 0x09, 0x06, 0x11, 0xFF], (2,))
print("pan-tilt max speed inq:", (f"pan {v[0]} tilt {v[1]}" if isinstance(v, (bytes, bytearray)) else v))

try:
    print("\n== zoom scale (Zoom Direct value -> reported) ==")
    send([0x81, 0x01, 0x04, 0x07, 0x02, 0xFF])  # tele
    zmax = settle(zoom)
    print(f"full tele reported: {zmax}")
    send([0x81, 0x01, 0x04, 0x07, 0x03, 0xFF])  # wide
    zmin = settle(zoom)
    print(f"full wide reported: {zmin}")
    for val in [500, 1000, 2000, 3000, 4000, 6000, 8000, 10000, 12000, 13000, 14000, 15000, 16000, 16384]:
        zoom_direct(val)
        r = settle(zoom)
        print(f"  direct {val:5d} -> {r:5d}  ratio {r / val:.4f}" if r else f"  direct {val} -> None")
    print("repeatability / direction (target 6000):")
    for frm in [2000, 10000, 2000, 10000]:
        zoom_direct(frm)
        settle(zoom)
        zoom_direct(6000)
        print(f"  from {frm:5d}: {settle(zoom)}")

    print("\n== pan/tilt landing accuracy (speed 12) ==")
    zoom_direct(int(start_zoom / ZOOM_SCALE))
    p0, t0 = start_pt
    targets = [(p0 + 300, t0), (p0 - 300, t0 + 60), (p0 + 37, t0 - 41), (p0, t0), (p0 + 5, t0 + 3), (p0, t0)]
    for tp, tt in targets:
        pt_abs(tp, tt)
        r = settle(pt)
        print(f"  target ({tp:5d},{tt:5d}) -> {r}  error ({r[0] - tp:+d},{r[1] - tt:+d})")

    print("\n== speed steps (units/s while driving) ==")
    def measure(axis, speed, direction, dur):
        idx = 0 if axis == "pan" else 1
        samples = []
        if axis == "pan":
            pt_drive(speed, 1, direction, 3)
        else:
            pt_drive(1, speed, 3, direction)
        t_end = time.monotonic() + dur
        while time.monotonic() < t_end:
            p = pt()
            if p:
                samples.append((time.monotonic(), p[idx]))
        pt_drive(1, 1, 3, 3)
        settle(pt, still=0.3)
        # skip acceleration, use the middle half
        n = len(samples)
        a, b = samples[n // 4], samples[(3 * n) // 4]
        return abs(b[1] - a[1]) / (b[0] - a[0]) if b[0] > a[0] else 0

    pt_abs(p0, t0)
    settle(pt)
    d = 1
    for s in [1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24]:
        dur = 1.6 if s <= 4 else 1.0 if s <= 12 else 0.7
        vel = measure("pan", s, d, dur)
        print(f"  pan  speed {s:2d}: {vel:8.1f}")
        d = 3 - d
        pt_abs(p0, t0)
        settle(pt)
    d = 1
    for s in [1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20]:
        dur = 1.2 if s <= 4 else 0.7 if s <= 12 else 0.5
        vel = measure("tilt", s, d, dur)
        print(f"  tilt speed {s:2d}: {vel:8.1f}")
        d = 3 - d
        pt_abs(p0, t0)
        settle(pt)
finally:
    print("\nrestoring start position")
    pt_drive(1, 1, 3, 3)
    pt_abs(start_pt[0], start_pt[1])
    zoom_direct(int(round(start_zoom / ZOOM_SCALE)))
    time.sleep(0.5)
    print(f"  pan/tilt {settle(pt)}, zoom {settle(zoom)}")
