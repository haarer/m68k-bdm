import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
FIRMWARE = str(PROJECT_ROOT / "main.hex")


def pytest_sessionstart(session):
    subprocess.run(
        ["st-flash", "--reset", "--format", "ihex", "write", FIRMWARE],
        cwd=PROJECT_ROOT,
        capture_output=True,
    )
