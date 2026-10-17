#!/usr/bin/env python3
"""Probes VISCA focus behaviour: sequence number echo, Focus Direct scale, zoom tracking in manual focus."""
import socket
import sys
import time

HOST = sys.argv[1]
PORT = int(sys.argv[2])
ZOOM_SCALE = float(sys.argv[3])

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(("0.0.0.0", 52381))
except OSError:
    sock.bind(("0.0.0.0", 0))
sock.settimeout(0.3)
seq = 1000


def send(payload, inquiry=False):
    global seq
    s = seq
    hdr = bytes([0x01, 0x10 if inquiry else 0x00, 0, len(payload)]) + s.to_bytes(4, "big")
    seq += 1
    sock.sendto(hdr + bytes(payload), (HOST, PORT))
    return s


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
    """Returns (data, sent seq, reply seq) for an inquiry reply whose data length is in sizes."""
    for _ in range(tries):
        drain()
        sent = send(payload, True)
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            try:
                data, _ = sock.recvfrom(1024)
            except socket.timeout:
                break
            has_hdr = len(data) >= 8 and data[0] == 0x01
            msg = data[8:] if has_hdr else data
            rseq = int.from_bytes(data[4:8], "big") if has_hdr else None
            if len(msg) >= 3 and (msg[1] & 0xF0) == 0x60:
                return ("error " + msg.hex(" "), sent, rseq)
            if len(msg) >= 3 and msg[1] == 0x50 and msg[-1] == 0xFF and len(msg) - 3 in sizes:
                return (msg[2:-1], sent, rseq)
    return (None, None, None)


def nib(b):
    v = 0
    for x in b:
        v = (v << 4) | (x & 0x0F)
    return v


def nibbles(v, n):
    return [(v >> (4 * (n - 1 - i))) & 0x0F for i in range(n)]


def zoom():
    d, _, _ = inquire([0x81, 0x09, 0x04, 0x47, 0xFF], (4,))
    return nib(d) if isinstance(d, (bytes, bytearray)) else None


def focus():
    d, _, _ = inquire([0x81, 0x09, 0x04, 0x48, 0xFF], (4,))
    return nib(d) if isinstance(d, (bytes, bytearray)) else d


def focus_mode():
    d, _, _ = inquire([0x81, 0x09, 0x04, 0x38, 0xFF], (1,))
    return {2: "auto", 3: "manual"}.get(d[0], hex(d[0])) if isinstance(d, (bytes, bytearray)) else d


def settle(read, timeout=15.0, still=0.6):
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


def zoom_direct(reported):
    send([0x81, 0x01, 0x04, 0x47] + nibbles(int(round(reported / ZOOM_SCALE)), 4) + [0xFF])


def focus_direct(v):
    send([0x81, 0x01, 0x04, 0x48] + nibbles(v, 4) + [0xFF])


def focus_manual():
    send([0x81, 0x01, 0x04, 0x38, 0x03, 0xFF])


def focus_auto():
    send([0x81, 0x01, 0x04, 0x38, 0x02, 0xFF])


print("== sequence number echo ==")
for name, payload, sizes in [
    ("power", [0x81, 0x09, 0x04, 0x00, 0xFF], (1,)),
    ("zoom", [0x81, 0x09, 0x04, 0x47, 0xFF], (4,)),
    ("focus", [0x81, 0x09, 0x04, 0x48, 0xFF], (4,)),
    ("focus mode", [0x81, 0x09, 0x04, 0x38, 0xFF], (1,)),
    ("pan/tilt", [0x81, 0x09, 0x06, 0x12, 0xFF], (8, 9)),
]:
    d, sent, rseq = inquire(payload, sizes)
    shown = d.hex(" ") if isinstance(d, (bytes, bytearray)) else d
    print(f"  {name:10s} sent seq {sent}, reply seq {rseq}, data {shown}")
# two inquiries back to back, to see whether replies can be told apart
drain()
s1 = send([0x81, 0x09, 0x04, 0x47, 0xFF], True)
s2 = send([0x81, 0x09, 0x04, 0x48, 0xFF], True)
print(f"  back to back: sent zoom seq {s1}, focus seq {s2}")
end = time.monotonic() + 0.5
while time.monotonic() < end:
    try:
        data, _ = sock.recvfrom(1024)
    except socket.timeout:
        break
    print(f"    reply seq {int.from_bytes(data[4:8], 'big')}: {data[8:].hex(' ')}")

start_zoom, start_focus, start_mode = zoom(), focus(), focus_mode()
print(f"\nstart: zoom {start_zoom}, focus {start_focus}, focus mode {start_mode}")
if not isinstance(start_focus, int) or start_zoom is None:
    sys.exit("no focus/zoom replies, aborting")

try:
    print("\n== Focus Direct scale (manual focus) ==")
    focus_manual()
    time.sleep(0.3)
    print(f"  focus mode after Manual: {focus_mode()}")
    for v in [0x1000, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, start_focus]:
        focus_direct(v)
        r = settle(focus)
        print(f"  direct {v:5d} (0x{v:04X}) -> {r}" + (f"  ratio {r / v:.4f}" if isinstance(r, int) and v else ""))

    print("\n== zoom tracking in manual focus ==")
    focus_direct(start_focus)
    f0 = settle(focus)
    for z in [start_zoom + 3000 if start_zoom < 12000 else start_zoom - 3000, start_zoom]:
        zoom_direct(z)
        zr = settle(zoom)
        print(f"  zoom -> {zr}: focus {settle(focus)} (was {f0})")

    print("\n== does Focus Direct during a zoom stick? ==")
    focus_direct(start_focus)
    settle(focus)
    target_f = start_focus + 0x800 if start_focus < 0xC000 else start_focus - 0x800
    zoom_direct(start_zoom + 3000 if start_zoom < 12000 else start_zoom - 3000)
    time.sleep(0.15)
    focus_direct(target_f)
    print(f"  sent Focus Direct {target_f} mid zoom -> zoom {settle(zoom)}, focus {settle(focus)}")
finally:
    print("\nrestoring")
    zoom_direct(start_zoom)
    settle(zoom)
    focus_direct(start_focus)
    settle(focus)
    if start_mode == "auto":
        focus_auto()
    time.sleep(0.5)
    print(f"  zoom {zoom()}, focus {focus()}, focus mode {focus_mode()}")
