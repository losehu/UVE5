"""List serial ports and open selected one at 115200 baud.
Usage:
  python scripts/serial_monitor.py
Requires:
  pip install pyserial
"""
import sys
import time
from typing import List, Any

try:
    import serial  # type: ignore
    from serial.tools import list_ports  # type: ignore
except ImportError:
    print("pyserial is required. Install with: pip install pyserial")
    sys.exit(1)

BAUD = 115200


def pick_port(ports: List[Any]) -> str:
    if not ports:
        print("No serial ports found.")
        sys.exit(1)
    for idx, p in enumerate(ports):
        print(f"[{idx}] {p.device} - {p.description}")
    while True:
        sel = input("Select port index: ").strip()
        if not sel.isdigit():
            print("Please enter a number.")
            continue
        i = int(sel)
        if 0 <= i < len(ports):
            return ports[i].device
        print("Index out of range.")


def main() -> None:
    ports = list(list_ports.comports())
    port = pick_port(ports)
    print(f"Opening {port} at {BAUD} baud. Ctrl+C to quit.")
    try:
        with serial.Serial(port, BAUD, timeout=1) as ser:
            while True:
                line = ser.readline()
                if line:
                    # decode with errors ignored to avoid blocking on bad bytes
                    print(line.decode(errors="ignore"), end="")
                else:
                    # no data; small sleep to avoid busy loop
                    time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
