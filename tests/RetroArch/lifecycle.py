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


def wait_for_file(path, expected_size, label, timeout=5.0):
    deadline = time.monotonic() + timeout
    last_size = None
    while time.monotonic() < deadline:
        try:
            last_size = os.path.getsize(path)
            if last_size == expected_size:
                return
        except FileNotFoundError:
            pass
        time.sleep(0.01)
    raise RuntimeError(
        f"timed out waiting for {label}; last size: {last_size!r}"
    )


def wait_for_logged(path, start, marker, label, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, encoding="utf-8", errors="replace") as log_file:
            log_file.seek(start)
            if marker in log_file.read():
                return
        time.sleep(0.01)
    raise RuntimeError(f"timed out waiting for {label}")


def run_core(
    sock,
    port,
    core,
    label,
    reset,
    save_state_path=None,
    load_state_path=None,
    log_path=None,
    unload=True,
):
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
    if save_state_path is not None:
        save_supported_state(sock, port, save_state_path, log_path)
    if load_state_path is not None:
        load_supported_state(sock, port, load_state_path, log_path)
    if not unload:
        return
    send(sock, port, "UNLOAD_CORE")
    wait_for(
        sock,
        port,
        lambda value: "CONTENTLESS" in value,
        f"{label} core unload",
    )
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


def save_supported_state(sock, port, state_path, log_path):
    log_start = os.path.getsize(log_path)
    send(sock, port, "SAVE_STATE_SLOT 0")
    wait_for_file(state_path, 112, "framed 88-byte serialized state")
    completed = (
        f'[INFO] [State] save task COMPLETED for slot 0, path "{state_path}" '
        "(112 bytes)."
    )
    wait_for_logged(
        log_path,
        log_start,
        completed,
        "RetroArch state-save task completion",
    )
    with open(state_path, "rb") as state_file:
        state = state_file.read()
    if (
        state[:8] != b"RASTATE\x01"
        or state[8:12] != b"MEM "
        or int.from_bytes(state[12:16], "little") != 88
        or state[16:20] != b"CKS1"
        or state[-8:] != b"END \x00\x00\x00\x00"
    ):
        raise RuntimeError("RetroArch state framing or core state header is invalid")
    probe(sock, port, "SAVE_STATE_SLOT 0")


def load_supported_state(sock, port, state_path, log_path):
    # START_CORE rebuilds the frontend drivers and command receiver. Let that
    # lifecycle transition settle before issuing state commands to the new one.
    time.sleep(1.0)
    log_start = os.path.getsize(log_path)
    send(sock, port, "LOAD_STATE")
    wait_for_logged(
        log_path,
        log_start,
        f'[INFO] [State] Loading state "{state_path}", 112 bytes.',
        "RetroArch state-load dispatch",
    )
    probe(sock, port, "LOAD_STATE")


def exercise_close_content(sock, port, core):
    send(sock, port, f"LOAD_CORE {core}")
    time.sleep(0.05)
    probe(sock, port, "close recovery LOAD_CORE")
    send(sock, port, "START_CORE")
    wait_for(sock, port, lambda value: "PLAYING" in value, "close recovery start")
    send(sock, port, "CLOSE_CONTENT")
    wait_for(
        sock,
        port,
        lambda value: "CONTENTLESS" in value,
        "content close",
    )

    # CLOSE_CONTENT completes through RetroArch's menu loop and then reloads
    # the selected core. Give that separate transition time to settle before
    # asking the frontend to start it again.
    time.sleep(1.0)
    send(sock, port, "START_CORE")
    wait_for(
        sock,
        port,
        lambda value: "PLAYING" in value,
        "restart after content close",
    )
    send(sock, port, "UNLOAD_CORE")
    wait_for(
        sock,
        port,
        lambda value: "CONTENTLESS" in value,
        "close recovery core unload",
    )
    probe(sock, port, "close recovery UNLOAD_CORE")


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
    parser.add_argument("--state", required=True)
    parser.add_argument("--save", required=True)
    parser.add_argument("--options", required=True)
    args = parser.parse_args()
    if args.cycles < 1:
        parser.error("--cycles must be positive")
    if args.rss_limit_mib < 0:
        parser.error("--rss-limit-mib cannot be negative")
    return args


def start_frontend(args, log, sock, disable_save_ram=False):
    command = [args.binary, "--verbose", "--config", args.config]
    if disable_save_ram:
        command.extend(("--sram-mode", "noload-nosave"))
    command.append("--menu")
    process = subprocess.Popen(
        command,
        stdout=log,
        stderr=subprocess.STDOUT,
    )
    try:
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline:
            version = ask(sock, args.port, "VERSION", 1.0)
            if version:
                return process
            if process.poll() is not None:
                raise RuntimeError(
                    f"RetroArch exited during startup: {process.returncode}"
                )
        raise RuntimeError("RetroArch did not enable its command interface")
    except Exception:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        raise


def quit_frontend(process, sock, port):
    send(sock, port, "QUIT")
    try:
        return_code = process.wait(timeout=10.0)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("RetroArch did not exit after QUIT") from error
    if return_code != 0:
        raise RuntimeError(f"RetroArch exited with status {return_code}")


def main():
    args = parse_args()
    for path in (args.log, args.state, args.save):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        try:
            os.remove(path)
        except FileNotFoundError:
            pass
    os.makedirs(os.path.dirname(args.options), exist_ok=True)
    with open(args.options, "w", encoding="utf-8") as options_file:
        options_file.write(
            'corekit_probe_palette = "monochrome"\n'
            'corekit_probe_tone = "off"\n'
        )
    log = open(args.log, "wb")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)
    process = None
    baseline_rss = None
    maximum_rss = 0
    try:
        process = start_frontend(args, log, sock, disable_save_ram=True)

        missing_content = f"{args.log}.missing-content"
        exercise_rejected_content(
            sock, args.port, args.managed_core, missing_content
        )
        print("rejected content load: recovered", flush=True)

        exercise_close_content(sock, args.port, args.control_core)
        print("content close and restart: recovered", flush=True)

        for cycle in range(1, args.cycles + 1):
            run_core(
                sock,
                args.port,
                args.managed_core,
                "managed",
                reset=True,
            )
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

        wait_for_logged(
            args.log,
            0,
            "[INFO] [Environ] SET_CONTROLLER_INFO.",
            "RetroArch controller-info registration",
        )
        wait_for_logged(
            args.log,
            0,
            "[libretro INFO] CoreKit controller port device forwarded",
            "RetroArch controller-port device forwarding",
        )
        wait_for_logged(
            args.log,
            0,
            "[libretro INFO] CoreKit tone option disabled",
            "RetroArch tone option application",
        )
        wait_for_logged(
            args.log,
            0,
            "[libretro INFO] CoreKit monochrome palette selected",
            "RetroArch palette option application",
        )
        print(
            "controller metadata, device forwarding, and core options: accepted",
            flush=True,
        )

        managed_mapped = module_is_mapped(process.pid, args.managed_core)
        quit_frontend(process, sock, args.port)

        process = start_frontend(args, log, sock)
        run_core(
            sock,
            args.port,
            args.managed_core,
            "managed state save",
            reset=False,
            save_state_path=args.state,
            log_path=args.log,
            unload=False,
        )
        quit_frontend(process, sock, args.port)
        wait_for_file(args.save, 64, "64-byte save RAM")
        with open(args.save, "rb") as save_file:
            if not any(save_file.read()):
                raise RuntimeError("save RAM remained unchanged after frame execution")
        print("save state, save RAM, and normal quit: accepted", flush=True)

        process = start_frontend(args, log, sock)
        run_core(
            sock,
            args.port,
            args.managed_core,
            "managed state load",
            reset=False,
            load_state_path=args.state,
            log_path=args.log,
            unload=False,
        )
        quit_frontend(process, sock, args.port)
        print("state load after process reopen: accepted", flush=True)

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
        print("PASS: RetroArch load/reset/unload/switch/state/save/quit lifecycle")
        return 0
    except Exception as error:
        status = process.poll() if process is not None else None
        if status is not None and status < 0:
            error = RuntimeError(f"{error}; RetroArch died on {signal.Signals(-status).name}")
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    finally:
        sock.close()
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log.close()


if __name__ == "__main__":
    sys.exit(main())
