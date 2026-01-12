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
        description="Generate src/app/font_blob.c from a font bin file."
    )
    parser.add_argument(
        "--input",
        default="./new_font_k.bin",
        help="Path to input .bin font file.",
    )
    parser.add_argument(
        "--output",
        default="../font_blob.c",
        help="Path to output .c file.",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    data = input_path.read_bytes()
    blob = format_bytes(data)

    content = (
        "/* Auto-generated from new_font_h.bin for embedded font storage. */\n"
        "#include \"font_blob.h\"\n\n"
        f"const unsigned int gFontBlobSize = {len(data)}u;\n"
        "const unsigned char gFontBlob[] = {\n"
        f"{blob}\n"
        "};\n\n"
        "void FONT_Read(uint32_t address, void *buffer, uint8_t size)\n"
        "{\n"
        "    if (!buffer || size == 0) {\n"
        "        return;\n"
        "    }\n"
        "    unsigned char *out = (unsigned char *)buffer;\n"
        "    for (unsigned int i = 0; i < size; ++i) {\n"
        "        out[i] = 0xFF;\n"
        "    }\n"
        "    if (address >= gFontBlobSize) {\n"
        "        return;\n"
        "    }\n"
        "    unsigned int available = gFontBlobSize - address;\n"
        "    unsigned int to_copy = available < size ? available : size;\n"
        "    for (unsigned int i = 0; i < to_copy; ++i) {\n"
        "        out[i] = gFontBlob[address + i];\n"
        "    }\n"
        "}\n"
    )

    output_path.write_text(content)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
