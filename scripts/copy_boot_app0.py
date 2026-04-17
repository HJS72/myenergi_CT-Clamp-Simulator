Import("env")

from pathlib import Path
import shutil


build_dir = Path(env.subst("$BUILD_DIR"))
framework_dir = Path(env.subst("$PROJECT_PACKAGES_DIR")) / "framework-arduinoespressif32"
source = framework_dir / "tools" / "partitions" / "boot_app0.bin"
target = build_dir / "boot_app0.bin"

if source.exists():
    shutil.copy2(source, target)
    print(f"[copy_boot_app0] copied {source} -> {target}")
else:
    print(f"[copy_boot_app0] source not found: {source}")
