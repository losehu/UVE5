Import("env")
import csv, os

def get_partition_size(partition_name, path):
    if not os.path.exists(path):
        print(f"⚠️ Partition file not found: {path}")
        return None
    with open(path) as f:
        for row in csv.reader(f):
            if not row or row[0].startswith("#"):
                continue
            name, typ, subtype, offset, size = row[:5]
            if name.strip() == partition_name:
                return int(size, 0)
    return None

def fix_size(source, target, env):
    partfile = env.GetProjectOption("board_build.partitions")
    partname = env.GetProjectOption("board_build.app_partition")
    size = get_partition_size(partname, partfile)
    if size:
        print(f"✅ Using partition '{partname}' size {size} bytes")
        # 旧方式（失效）
        # env["BOARD_UPLOAD"]["maximum_size"] = size
        # ✅ 新方式（PlatformIO 6+）
        env["UPLOAD_MAXIMUM_SIZE"] = size
        env.BoardConfig().update("upload.maximum_size", str(size))
    else:
        print(f"⚠️ Could not find partition {partname} in {partfile}")

env.AddPreAction("checkprogsize", fix_size)
