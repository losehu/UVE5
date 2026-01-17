#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import struct
import sys
import time
from typing import Optional, Tuple

try:
    import serial  # pyserial
except Exception as exc:
    serial = None
    _import_error = exc


K5_XOR_ARRAY = bytes([
    0x16, 0x6C, 0x14, 0xE6, 0x2E, 0x91, 0x0D, 0x40,
    0x21, 0x35, 0xD5, 0x40, 0x13, 0x03, 0xE9, 0x80,
])

HEADER = b"\xAB\xCD"
FOOTER_ID = b"\xDC\xBA"


def crc16_xmodem(data: bytes, crc: int = 0) -> int:
    poly = 0x1021
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ poly) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


def xor_k5(data: bytes) -> bytes:
    out = bytearray(data)
    for i in range(len(out)):
        out[i] ^= K5_XOR_ARRAY[i % len(K5_XOR_ARRAY)]
    return bytes(out)


def packetize(payload: bytes, *, xor_enable: bool = True) -> bytes:
    # Matches k5web/src/utils/serial.js by default (xor_enable=True):
    # - length is payload length (does NOT include CRC)
    # - CRC16-XMODEM over payload, little-endian appended
    # - XOR applied to (payload + CRC)
    # - footer id is 0xDC 0xBA
    length = struct.pack('<H', len(payload))
    crc = crc16_xmodem(payload)
    crc_le = struct.pack('<H', crc)
    body = payload + crc_le
    if xor_enable:
        body = xor_k5(body)
    return HEADER + length + body + FOOTER_ID


def try_parse_one_packet(buffer: bytearray) -> Optional[Tuple[bytes, bytes]]:
    """Return (body_raw, raw_packet) if a full packet is available; else None.

    Packet length on wire is always: payload_len + 8
      header(2) + length(2) + payload_len + trailer(4)

    Trailer differs depending on direction:
      - host->radio: xor(crc16(payload)) + footer_id
      - radio->host: padding(2) + footer_id

    For handshake test we only need to decode payload.
    """
    # Find header
    idx = buffer.find(HEADER)
    if idx < 0:
        buffer.clear()
        return None
    if idx > 0:
        del buffer[:idx]

    if len(buffer) < 4:
        return None

    payload_len = buffer[2] | (buffer[3] << 8)
    total_len = payload_len + 8
    if len(buffer) < total_len:
        return None

    pkt = bytes(buffer[:total_len])
    # Basic footer ID check (last 2 bytes)
    if pkt[-2:] != FOOTER_ID:
        # resync by dropping one byte
        del buffer[0:1]
        return None

    del buffer[:total_len]
    body_raw = pkt[4:4 + payload_len]
    return body_raw, pkt


def decode_body(body_raw: bytes) -> Tuple[bytes, str]:
    """Try XOR and plain, pick the one that looks like a valid inner header."""
    if len(body_raw) >= 2:
        de_xor = xor_k5(body_raw)
        # Handshake reply inner ID = 0x0515 -> bytes 15 05
        if len(de_xor) >= 2 and de_xor[0] == 0x15 and de_xor[1] == 0x05:
            return de_xor, 'xor'
        if body_raw[0] == 0x15 and body_raw[1] == 0x05:
            return body_raw, 'plain'
    return xor_k5(body_raw), 'xor?'  # best effort


def read_packet(ser, expected_first_byte: Optional[int], timeout_s: float, verbose: bool) -> bytes:
    deadline = time.monotonic() + timeout_s
    buf = bytearray()

    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf.extend(chunk)

        parsed = try_parse_one_packet(buf)
        if parsed is None:
            continue

        body_raw, raw_pkt = parsed
        decoded, mode = decode_body(body_raw)
        if verbose:
            print(f"[RX] raw={raw_pkt.hex()} body_raw={body_raw.hex()} decoded({mode})={decoded.hex()}")

        if expected_first_byte is None or (len(decoded) > 0 and decoded[0] == expected_first_byte):
            return decoded

    raise TimeoutError("Timeout waiting for packet")


def send_packet(ser, payload: bytes, verbose: bool, *, xor_enable: bool = True) -> None:
    pkt = packetize(payload, xor_enable=xor_enable)
    if verbose:
        print(f"[TX] payload={payload.hex()} pkt={pkt.hex()}")
    ser.write(pkt)
    ser.flush()


def handshake(port: str, baud: int, timeout_s: float, verbose: bool, *, tx_plain: bool = False, settle_s: float = 1.0, no_tx: bool = False, hold_s: float = 0.0) -> int:
    if serial is None:
        raise RuntimeError(
            f"pyserial 未安装或导入失败: {_import_error}.\n"
            f"请先执行: python3 -m pip install pyserial"
        )

    with serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.05,
        rtscts=False,
        dsrdtr=False,
    ) as ser:
        # Many USB-UART adapters toggle DTR/RTS on open, which can reset ESP32.
        # Force them low and give the target a moment to boot.
        try:
            ser.dtr = False
            ser.rts = False
        except Exception:
            pass

        time.sleep(max(0.0, settle_s))
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        if no_tx:
            if verbose:
                print(f"[OPEN] opened {port} baud={baud}; no-tx mode; holding {hold_s:.2f}s")
            deadline = time.monotonic() + max(0.0, hold_s)
            while time.monotonic() < deadline:
                chunk = ser.read(ser.in_waiting or 1)
                if chunk and verbose:
                    print(f"[RX] {chunk.hex()}")
            return 0

        # Matches k5web eeprom_init():
        # [0x14,0x05] ID=0x0514
        # [0x04,0x00] length=4
        # timestamp = 0xFFFFFFFF
        payload = bytes([0x14, 0x05, 0x04, 0x00, 0xFF, 0xFF, 0xFF, 0xFF])

        resp = None
        for attempt in range(3):
            send_packet(ser, payload, verbose=verbose, xor_enable=(not tx_plain))
            # Expect reply ID 0x0515 -> first byte 0x15
            try:
                resp = read_packet(ser, expected_first_byte=0x15, timeout_s=timeout_s, verbose=verbose)
                break
            except TimeoutError:
                if verbose:
                    print(f"[RETRY] no reply yet (attempt {attempt + 1}/3)")
                # If opening the port reset the ESP32, give it more time before retrying.
                time.sleep(max(0.0, settle_s))
                continue

        if resp is None:
            raise TimeoutError("Timeout waiting for packet")

        if len(resp) < 4 + 16:
            raise RuntimeError(f"Response too short: {len(resp)} bytes")

        version_raw = resp[4:4 + 16]
        version = version_raw.split(b'\x00', 1)[0].decode('ascii', errors='replace')
        print(f"[OK] baud={baud} tx={'plain' if tx_plain else 'xor'} rx=auto version={version!r}")
        return 0


def raw_send(port: str, baud: int, data: bytes, seconds: float, interval_s: float, verbose: bool, settle_s: float) -> int:
    if serial is None:
        raise RuntimeError(
            f"pyserial 未安装或导入失败: {_import_error}.\n"
            f"请先执行: python3 -m pip install pyserial"
        )

    with serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.05,
        rtscts=False,
        dsrdtr=False,
    ) as ser:
        try:
            ser.dtr = False
            ser.rts = False
        except Exception:
            pass

        time.sleep(max(0.0, settle_s))
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        if verbose:
            print(f"[RAW] opened {port} baud={baud}; sending {data.hex()} for {seconds:.2f}s every {interval_s:.3f}s")

        deadline = time.monotonic() + max(0.0, seconds)
        while time.monotonic() < deadline:
            ser.write(data)
            ser.flush()
            # also drain any inbound bytes for debug
            chunk = ser.read(ser.in_waiting or 1)
            if chunk and verbose:
                print(f"[RX] {chunk.hex()}")
            time.sleep(max(0.0, interval_s))
        return 0


def handshake_scan(port: str, bauds: list[int], timeout_s: float, verbose: bool, *, tx_plain: bool, settle_s: float) -> int:
    last_err: Optional[Exception] = None
    for b in bauds:
        try:
            if verbose:
                print(f"[SCAN] trying baud={b} tx={'plain' if tx_plain else 'xor'}")
            return handshake(port, b, timeout_s, verbose, tx_plain=tx_plain, settle_s=settle_s)
        except Exception as e:
            last_err = e
            if verbose:
                print(f"[SCAN] baud={b} failed: {e}")
            continue
    raise RuntimeError(f"All bauds failed. Last error: {last_err}")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="K5 协议握手测试（复刻 k5web 的 packetize/xor/crc16xmodem）")
    ap.add_argument('--port', default='/dev/cu.usbserial-1130', help='串口设备，例如 /dev/cu.usbserial-1130')
    ap.add_argument('--baud', type=int, default=38400, help='波特率（k5web 默认 38400）')
    ap.add_argument('--scan', action='store_true', help='扫描常见波特率（忽略 --baud）')
    ap.add_argument('--tx-plain', action='store_true', help='发送时不做 XOR（用于排查某些固件/模式）')
    ap.add_argument('--timeout', type=float, default=1.5, help='等待回复超时时间（秒）')
    ap.add_argument('--settle', type=float, default=1.0, help='打开串口后等待目标稳定的时间（秒），用于规避 DTR/RTS 触发的 ESP32 复位')
    ap.add_argument('--no-tx', action='store_true', help='只打开串口不发送任何数据（用于判断是否“打开端口”就触发复位）')
    ap.add_argument('--hold', type=float, default=2.0, help='--no-tx 模式下保持打开的时间（秒）')
    ap.add_argument('--raw', default=None, help='发送原始十六进制字节（不走协议），例如: 55 或 abcd')
    ap.add_argument('--raw-seconds', type=float, default=2.0, help='--raw 模式持续发送时长（秒）')
    ap.add_argument('--raw-interval', type=float, default=0.05, help='--raw 模式发送间隔（秒）')
    ap.add_argument('-v', '--verbose', action='store_true', help='打印收发的原始十六进制数据')
    args = ap.parse_args(argv)

    try:
        if args.raw is not None:
            hex_str = args.raw.strip().lower().replace(' ', '')
            if len(hex_str) % 2 == 1:
                hex_str = '0' + hex_str
            data = bytes.fromhex(hex_str)
            return raw_send(args.port, args.baud, data, args.raw_seconds, args.raw_interval, args.verbose, args.settle)
        if args.scan:
            return handshake_scan(args.port, [38400, 115200, 57600, 19200, 9600], args.timeout, args.verbose, tx_plain=args.tx_plain, settle_s=args.settle)
        return handshake(args.port, args.baud, args.timeout, args.verbose, tx_plain=args.tx_plain, settle_s=args.settle, no_tx=args.no_tx, hold_s=args.hold)
    except Exception as e:
        print(f"[FAIL] {e}", file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
