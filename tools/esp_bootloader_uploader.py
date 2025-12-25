#!/usr/bin/env python3
"""
ESP bootloader uploader
- 自动列出串口并让你选择
- 按照 `src/bootloader/main.cpp` 的协议发送：
  - 等待 READY -> 发送 "GO"
  - 发送 header: MAGIC (0x32445055) + total(uint32 LE) + chunk(uint16 LE)
  - 等待 START
  - 按块发送: seq(uint32 LE) + len(uint16 LE) + data + crc32(uint32 LE)
  - 等待设备返回 'A' (ACK) 或 'E'<code> (NAK)

Usage:
  python3 tools/esp_bootloader_uploader.py --file firmware.bin
"""

import argparse
import os
import struct
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except Exception as e:
    print("请先安装 pyserial: pip install pyserial")
    raise

MAX_CHUNK = 2048
MAGIC = 0x32445055
MAX_FW_SIZE = 3 * 1024 * 1024


# CRC32 (与设备实现一致)
def crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return (~crc) & 0xFFFFFFFF


def list_serial_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("未找到串口设备")
        return []
    for i, p in enumerate(ports):
        print(f"[{i}] {p.device} - {p.description}")
    return ports


def read_line(ser, timeout=30.0):
    end = time.time() + timeout
    line = b""
    while time.time() < end:
        c = ser.read(1)
        if not c:
            continue
        line += c
        if c == b"\n":
            try:
                return line.decode(errors='ignore').strip()
            except:
                return line.strip()
    return None


def read_byte(ser, timeout=10.0):
    end = time.time() + timeout
    while time.time() < end:
        b = ser.read(1)
        if b:
            return b
    return None


def upload(port, baud, file_path, chunk_size=1024, max_retries=5):
    if chunk_size <= 0 or chunk_size > MAX_CHUNK:
        print(f"chunk must be between 1 and {MAX_CHUNK}")
        return 2

    size = os.path.getsize(file_path)
    if size < 16 or size > MAX_FW_SIZE:
        print("固件大小不合规")
        return 2

    if size > MAX_FW_SIZE:
        print("固件超过最大允许大小")
        return 2

    print(f"打开串口 {port} @ {baud} ...")
    ser = serial.Serial(port, baudrate=baud, timeout=0.5)
    time.sleep(0.1)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    print("等待设备发 READY（最长 60s）...")
    ready_seen = False
    t0 = time.time()
    while time.time() - t0 < 60:
        line = read_line(ser, timeout=1.0)
        if line:
            print("<", line)
            if "READY" in str(line).upper():
                ready_seen = True
                break
    if not ready_seen:
        print("未收到 READY，仍会尝试发送 GO（你可以手动按下 boot 按钮再次尝试）")

    # 发送 GO
    print("> GO")
    ser.write(b"GO\n")
    ser.flush()

    # 发送 header
    print(f"发送 header: magic=0x{MAGIC:08X}, total={size}, chunk={chunk_size}")
    header = struct.pack('<I I H', MAGIC, size, chunk_size)
    ser.write(header)
    ser.flush()

    # 等待 START
    print("等待设备 START...")
    start_seen = False
    t0 = time.time()
    while time.time() - t0 < 10:
        line = read_line(ser, timeout=1.0)
        if line:
            print("<", line)
            if "START" in str(line).upper():
                start_seen = True
                break
    if not start_seen:
        print("未收到 START，继续尝试（设备可能已进入 START 但超时）")

    # 开始传输
    with open(file_path, 'rb') as f:
        total = size
        offset = 0
        seq = 0
        while offset < total:
            f.seek(offset)
            data = f.read(min(chunk_size, total - offset))
            if not data:
                break
            crc = crc32(data)
            packet = struct.pack('<I H', seq, len(data)) + data + struct.pack('<I', crc)

            attempt = 0
            while attempt < max_retries:
                attempt += 1
                ser.write(packet)
                ser.flush()

                # 等待单字节 ACK/ERR
                rsp = read_byte(ser, timeout=20.0)
                if not rsp:
                    print(f"超时：未收到ACK（第 {seq} 包，重试 {attempt}/{max_retries}）")
                    continue
                if rsp == b'A':
                    offset += len(data)
                    seq += 1
                    pct = (offset * 100) // total
                    print(f"已写 {offset}/{total} 字节 ({pct}%)")
                    break
                elif rsp == b'E':
                    code = read_byte(ser, timeout=1.0)
                    code_val = code[0] if code else None
                    print(f"设备返回错误 E code={code_val}（第 {seq} 包），重试 {attempt}/{max_retries}")
                    # 读掉任何残余
                    continue
                else:
                    print(f"收到未知响应: {rsp}（第 {seq} 包），重试 {attempt}/{max_retries}")
                    continue

            if attempt >= max_retries:
                print("超过最大重试次数，终止传输")
                ser.close()
                return 3

    print("数据发送完成，等待设备 OK...")
    # 读取直到看到 OK 或 ERR
    t0 = time.time()
    while time.time() - t0 < 10:
        line = read_line(ser, timeout=1.0)
        if line:
            print("<", line)
            if "OK" in str(line).upper():
                print("设备返回 OK，上传成功。")
                ser.close()
                return 0
            if "ERR" in str(line).upper():
                print("设备返回 ERR，可能需要重试")
                break
    ser.close()
    print("未检测到明确的 OK，检查设备日志以确认结果")
    return 0


def main():
    p = argparse.ArgumentParser(description='ESP bootloader serial uploader')
    p.add_argument('--file', '-f', required=True, help='固件文件路径')
    p.add_argument('--port', '-p', help='串口设备，比如 /dev/ttyUSB0')
    p.add_argument('--baud', '-b', type=int, default=115200)
    p.add_argument('--chunk', type=int, default=1024, help=f'分块大小（<= {MAX_CHUNK}）')
    args = p.parse_args()

    if not os.path.exists(args.file):
        print('固件文件不存在:', args.file)
        sys.exit(2)

    port = args.port
    if not port:
        ports = list_serial_ports()
        if not ports:
            sys.exit(1)
        idx = input('请选择串口序号: ')
        try:
            i = int(idx)
            port = ports[i].device
        except Exception as e:
            print('选择无效')
            sys.exit(1)

    rc = upload(port, args.baud, args.file, chunk_size=args.chunk)
    sys.exit(rc)


if __name__ == '__main__':
    main()
