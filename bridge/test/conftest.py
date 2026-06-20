import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def pytest_sessionstart(session):
    subprocess.run(
        ["st-flash", "reset"],
        cwd=PROJECT_ROOT,
        capture_output=True,
    )
