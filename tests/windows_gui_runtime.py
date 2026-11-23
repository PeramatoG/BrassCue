"""Run a Windows GUI executable without vcpkg or IDE directories on PATH."""

import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    timeout, executable, *args = sys.argv[1:]
    executable = Path(executable).resolve(strict=True)
    windows = Path(os.environ["SystemRoot"])
    env = dict(os.environ, PATH=os.pathsep.join(map(str, (windows / "System32", windows))))

    # Inherited by the child. A missing DLL must produce an exit status, not a dialog.
    set_error_mode = ctypes.windll.kernel32.SetErrorMode
    previous = set_error_mode(0x0001 | 0x0002 | 0x8000)
    try:
        with tempfile.TemporaryDirectory(prefix="brasscue-gui-") as directory:
            result = subprocess.run(
                [str(executable), *args], cwd=directory, env=env, timeout=int(timeout)
            )
    except subprocess.TimeoutExpired:
        print(f"{executable.name} did not exit in {timeout} seconds", file=sys.stderr)
        return 1
    finally:
        set_error_mode(previous)

    if result.returncode not in (0, 77):
        print(
            f"{executable.name} exited with 0x{result.returncode & 0xFFFFFFFF:08X}",
            file=sys.stderr,
        )
        return 1
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
