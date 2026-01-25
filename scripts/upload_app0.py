from SCons.Script import DefaultEnvironment
import os

env = DefaultEnvironment()
platform = env.PioPlatform()

# 你的 app0 起始地址（务必与 part.csv 一致！）
APP0_OFFSET = 0x0F0000

# ESP32 partition table offset (standard)
PARTITIONS_OFFSET = 0x8000

# esptool.py 路径
esptool_dir = platform.get_package_dir("tool-esptoolpy")
esptool_py = os.path.join(esptool_dir, "esptool.py")

# Build artifacts
build_dir = env.subst("$BUILD_DIR")
partitions_bin = os.path.join(build_dir, "partitions.bin")

# 组合上传命令：直接 write_flash 到 app0
env.Replace(
    UPLOADER="$PYTHONEXE " + esptool_py,
    UPLOADERFLAGS=[
        "--chip", "esp32s3",
        "--port", "$UPLOAD_PORT",
        "--baud", "$UPLOAD_SPEED",
        "--before", "default_reset",
        "--after",  "hard_reset",
    ],
    # $SOURCE is .pio/build/app0_main/firmware.bin
    # Also flash partitions.bin so the "shared" data partition exists.
    # Without this, shared_read/shared_write will silently fall back to zeros and TLE will appear missing.
    UPLOADCMD='$UPLOADER $UPLOADERFLAGS write_flash '
              '{part_off:#x} {part_bin} '
              '{app_off:#x} $SOURCE'.format(
                  part_off=PARTITIONS_OFFSET,
                  part_bin=partitions_bin,
                  app_off=APP0_OFFSET,
              )
)
