"""Hardware regression check: run with the StopWatch on its FX screen, USB powered.

Usage: python serial_connection.py /dev/cu.usbmodemXXXX [seconds] [--allow-maintenance]
Opening the USB serial port may reset the device. Requires pyserial.
With --allow-maintenance, an authenticated HTTP 200/API 5 response also passes
the connectivity check; it does not prove that quotes are available.
"""
import re
import sys
import time

import serial

port = serial.Serial()
port.port = sys.argv[1]
port.baudrate = 115200
port.timeout = 0.5
port.dtr = False
port.rts = False
port.open()
deadline = time.monotonic() + (float(sys.argv[2]) if len(sys.argv) > 2 else 45)
next_query = 0
pending = b""
successful = 0
maintenance = 0
boots = 0
try:
    while time.monotonic() < deadline:
        if time.monotonic() >= next_query:
            port.write(b"?")
            next_query = time.monotonic() + 2
        pending += port.read(4096)
        while b"\n" in pending:
            raw, pending = pending.split(b"\n", 1)
            line = raw.decode(errors="replace").strip()
            if "watchdog got triggered" in line or "Guru Meditation" in line:
                raise SystemExit("FAIL: firmware crashed during connection")
            if line.startswith("STOPWATCH FX HOTSPOT"):
                boots += 1
                print(line, flush=True)
                if boots > 1:
                    raise SystemExit("FAIL: firmware rebooted")
            if line.startswith("FX HOTSPOT version="):
                print(line, flush=True)
                if re.search(r"network=[01] state=2 quote=1\b", line):
                    successful += 1
                if re.search(r"network=[01] state=7\b", line) and "http=200 api=5 cpu=80" in line:
                    maintenance += 1
    if "--allow-maintenance" in sys.argv and maintenance >= 3:
        print(f"PASS: {maintenance} API maintenance samples; CPU restored; no crash; quotes unavailable")
    elif successful < 3:
        raise SystemExit("FAIL: fewer than three connected, quote-ready samples")
    else:
        print(f"PASS: {successful} connected, quote-ready samples; no crash")
finally:
    port.close()
