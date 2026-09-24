"""Keep a parent process for xvfb-run in Docker; Xvfb will not signal PID 1 as ready."""
import os
import signal
import subprocess
import sys

if len(sys.argv) < 2:
    raise SystemExit('usage: run_with_xvfb.py command [arguments...]')

# Alpine's shell may exec the final Docker RUN command as PID 1. A Python parent
# prevents xvfb-run becoming PID 1 and bounds startup/command execution time.
process = subprocess.Popen(['xvfb-run', '-a', *sys.argv[1:]], start_new_session=True)
try:
    raise SystemExit(process.wait(timeout=300))
except subprocess.TimeoutExpired:
    print('Xvfb test command exceeded 300 seconds', file=sys.stderr)
    raise SystemExit(1)
finally:
    # Also reap the display server if xvfb-run fails during startup.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    if process.poll() is None:
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
