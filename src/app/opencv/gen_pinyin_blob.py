#!/usr/bin/env python3
import argparse
from pathlib import Path


def format_bytes(data: bytes, per_line: int = 12) -> str:
    parts = [f"0x{b:02X}" for b in data]
    lines = []
    for i in range(0, len(parts), per_line):
        chunk = parts[i:i + per_line]
        lines.append("    " + ", ".join(chunk) + ",")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate src/app/pinyin_blob.c from a pinyin bin file."
    )
    parser.add_argument(
        "--input",
        default="./pinyin_plus.bin",
        help="Path to input .bin pinyin file.",
    )
    parser.add_argument(
        "--output",
        default="../pinyin_blob.c",
        help="Path to output .c file.",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    data = input_path.read_bytes()
    blob = format_bytes(data)

    content = (
        "/* Auto-generated from pinyin_plus.bin for embedded pinyin storage. */\n"
        "#include \"pinyin_blob.h\"\n\n"
        f"const unsigned int gPinyinBlobSize = {len(data)}u;\n"
        "const unsigned char gPinyinBlob[] = {\n"
        f"{blob}\n"
        "};\n\n"
        "void PINYIN_Read(uint32_t address, void *buffer, uint8_t size)\n"
        "{\n"
        "    if (!buffer || size == 0) {\n"
        "        return;\n"
        "    }\n"
        "    unsigned char *out = (unsigned char *)buffer;\n"
        "    for (unsigned int i = 0; i < size; ++i) {\n"
        "        out[i] = 0xFF;\n"
        "    }\n"
        "    if (address < PINYIN_BLOB_BASE) {\n"
        "        return;\n"
        "    }\n"
        "    uint32_t offset = address - PINYIN_BLOB_BASE;\n"
        "    if (offset >= gPinyinBlobSize) {\n"
        "        return;\n"
        "    }\n"
        "    unsigned int available = gPinyinBlobSize - offset;\n"
        "    unsigned int to_copy = available < size ? available : size;\n"
        "    for (unsigned int i = 0; i < to_copy; ++i) {\n"
        "        out[i] = gPinyinBlob[offset + i];\n"
        "    }\n"
        "}\n"
    )

    output_path.write_text(content)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
