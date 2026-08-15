#!/usr/bin/env python3
"""Focused RetroArch lifecycle gate for the Phase 0 contentless cores."""

import argparse
import os
import signal
import socket
import subprocess
import sys
import time


def send(sock, port, command):
    sock.sendto(command.encode("utf-8"), ("127.0.0.1", port))


def ask(sock, port, command, timeout=5.0):
    while True:
        try:
            sock.recvfrom(65535)
        except BlockingIOError:
            break
    send(sock, port, command)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            return sock.recvfrom(65535)[0].decode("utf-8", "replace").strip()
        except BlockingIOError:
            time.sleep(0.005)
    return None


def wait_for(sock, port, predicate, label, timeout=15.0):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = ask(sock, port, "GET_STATUS", 2.0)
        if last and predicate(last):
            return
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {label}; last status: {last!r}")


def probe(sock, port, label):
    version = ask(sock, port, "VERSION")
    if not version:
        raise RuntimeError(f"RetroArch stopped responding after {label}")


def read_rss_kib(pid):
    try:
        with open(f"/proc/{pid}/status", encoding="ascii") as status:
            for line in status:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except (FileNotFoundError, PermissionError):
        pass
    return None


def module_is_mapped(pid, path):
    try:
        with open(f"/proc/{pid}/maps", encoding="utf-8") as maps:
            return any(path in line for line in maps)
    except (FileNotFoundError, PermissionError):
        return None


def run_core(sock, port, core, label, reset, unsupported_state=False):
    send(sock, port, f"LOAD_CORE {core}")
    time.sleep(0.05)
    probe(sock, port, f"{label} LOAD_CORE")
    send(sock, port, "START_CORE")
    wait_for(sock, port, lambda value: "PLAYING" in value, f"{label} core start")
    time.sleep(0.05)
    if reset:
        send(sock, port, "RESET")
        time.sleep(0.05)
        probe(sock, port, f"{label} RESET")
    if unsupported_state:
        exercise_unsupported_state(sock, port)
    send(sock, port, "CLOSE_CONTENT")
    wait_for(
        sock,
        port,
        lambda value: "CONTENTLESS" in value,
        f"{label} content close",
    )
    send(sock, port, "UNLOAD_CORE")
    time.sleep(0.05)
    probe(sock, port, f"{label} UNLOAD_CORE")


def exercise_rejected_content(sock, port, core, missing_content):
    if os.path.exists(missing_content):
        raise RuntimeError(
            f"rejected-content fixture unexpectedly exists: {missing_content}"
        )
    send(sock, port, f"LOAD_CONTENT {core}|{missing_content}")
    time.sleep(1.0)
    status = ask(sock, port, "GET_STATUS")
    if not status or "CONTENTLESS" not in status:
        raise RuntimeError(f"failed content load left unexpected status: {status!r}")
    probe(sock, port, "rejected content load")


def exercise_unsupported_state(sock, port):
    for command in ("SAVE_STATE", "LOAD_STATE"):
        send(sock, port, command)
        time.sleep(0.25)
        probe(sock, port, f"unsupported {command}")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--managed-core", required=True)
    parser.add_argument("--control-core", required=True)
    parser.add_argument("--cycles", type=int, default=50)
    parser.add_argument("--rss-limit-mib", type=int, default=16)
    parser.add_argument("--port", type=int, default=55355)
    parser.add_argument("--log", required=True)
    args = parser.parse_args()
    if args.cycles < 1:
        parser.error("--cycles must be positive")
    if args.rss_limit_mib < 0:
        parser.error("--rss-limit-mib cannot be negative")
    return args


def main():
    args = parse_args()
    os.makedirs(os.path.dirname(args.log), exist_ok=True)
    log = open(args.log, "wb")
    process = subprocess.Popen(
        [args.binary, "--verbose", "--config", args.config, "--menu"],
        stdout=log,
        stderr=subprocess.STDOUT,
    )
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)
    baseline_rss = None
    maximum_rss = 0
    try:
        deadline = time.monotonic() + 20.0
        version = None
        while time.monotonic() < deadline:
            version = ask(sock, args.port, "VERSION", 1.0)
            if version:
                break
            if process.poll() is not None:
                raise RuntimeError(f"RetroArch exited during startup: {process.returncode}")
        if not version:
            raise RuntimeError("RetroArch did not enable its command interface")

        missing_content = f"{args.log}.missing-content"
        exercise_rejected_content(
            sock, args.port, args.managed_core, missing_content
        )
        print("rejected content load: recovered", flush=True)

        for cycle in range(1, args.cycles + 1):
            run_core(
                sock,
                args.port,
                args.managed_core,
                "managed",
                reset=True,
                unsupported_state=cycle == 1,
            )
            if cycle == 1:
                print("unsupported save/load state commands: recovered", flush=True)
            run_core(sock, args.port, args.control_core, "control", reset=False)
            rss = read_rss_kib(process.pid)
            if rss is not None:
                if baseline_rss is None:
                    baseline_rss = rss
                maximum_rss = max(maximum_rss, rss)
            print(
                f"cycle {cycle}/{args.cycles}: rss={rss or 'unavailable'} KiB",
                flush=True,
            )

        managed_mapped = module_is_mapped(process.pid, args.managed_core)
        send(sock, args.port, "QUIT")
        try:
            return_code = process.wait(timeout=10.0)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError("RetroArch did not exit after QUIT") from error
        if return_code != 0:
            raise RuntimeError(f"RetroArch exited with status {return_code}")

        growth_kib = maximum_rss - baseline_rss if baseline_rss is not None else None
        print(f"managed core mapped after unload: {managed_mapped}")
        if growth_kib is not None:
            print(
                f"RSS after warm-up: {baseline_rss / 1024:.2f} MiB; "
                f"peak: {maximum_rss / 1024:.2f} MiB; "
                f"growth: {growth_kib / 1024:.2f} MiB"
            )
            if growth_kib > args.rss_limit_mib * 1024:
                raise RuntimeError(
                    f"RSS growth exceeded {args.rss_limit_mib} MiB limit"
                )
        print("PASS: RetroArch load/reset/close/unload/switch/quit lifecycle")
        return 0
    except Exception as error:
        status = process.poll()
        if status is not None and status < 0:
            error = RuntimeError(f"{error}; RetroArch died on {signal.Signals(-status).name}")
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    finally:
        sock.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log.close()


if __name__ == "__main__":
    sys.exit(main())
