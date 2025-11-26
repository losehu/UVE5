from SCons.Script import DefaultEnvironment
import os

env = DefaultEnvironment()
platform = env.PioPlatform()

# 你的 app0 起始地址（务必与 part.csv 一致！）
APP0_OFFSET = 0x0F0000

# esptool.py 路径
esptool_dir = platform.get_package_dir("tool-esptoolpy")
esptool_py = os.path.join(esptool_dir, "esptool.py")

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
    # $SOURCE 是 .pio/build/app0_main/firmware.bin
    UPLOADCMD='$UPLOADER $UPLOADERFLAGS write_flash {:#x} $SOURCE'.format(APP0_OFFSET)
)
