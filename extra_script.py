#!/usr/bin/env python3
import os, shutil
from datetime import datetime
Import("env")

date_str = datetime.now().strftime("%Y-%m-%d")

# Generate version.h with compile date
src_dir = env.subst("$PROJECTSRC_DIR")
with open(os.path.join(src_dir, "version.h"), "w") as f:
    f.write(f'#pragma once\n#define FW_VERSION "{date_str}"\n')
print(f"[BUILD] Generated version.h → FW_VERSION = {date_str}")

# Copy firmware.bin → YYYY-MM-DD_firmware.bin after build
def _rename_fw(source, target, env):
    src = source[0].path  # path to the built .bin
    dst = os.path.join(os.path.dirname(src), f"{date_str}_firmware.bin")
    if os.path.exists(src):
        shutil.copy2(src, dst)
        print(f"[BUILD] Firmware → {os.path.basename(dst)}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _rename_fw)
