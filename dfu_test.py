#!/usr/bin/env python3
"""Send UPDATE over USB CDC and verify the STM32 enters ROM DFU mode."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parent
CDC_VID = 0xCAFE
CDC_PID = 0x4010
DFU_INSTANCE_ID = "VID_0483&PID_DF11"


def build_firmware():
    command_processor = os.environ.get("COMSPEC", "cmd.exe")
    subprocess.run(
        [command_processor, "/c", "build.bat", "usb"],
        cwd=ROOT,
        check=True,
    )


def find_cdc_port(list_ports, requested_port):
    if requested_port:
        return requested_port

    matches = [
        port.device
        for port in list_ports.comports()
        if port.vid == CDC_VID and port.pid == CDC_PID
    ]
    if len(matches) > 1:
        raise RuntimeError(
            "More than one USB Update CDC device was found; specify --port."
        )
    return matches[0] if matches else None


def bootloader_is_present():
    result = subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            "Get-PnpDevice -PresentOnly | ForEach-Object { $_.InstanceId }",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
        timeout=10,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "Could not inspect Windows USB devices: {}".format(result.stdout.strip())
        )
    return DFU_INSTANCE_ID in result.stdout.upper()


def wait_for_bootloader(timeout_seconds):
    print("Waiting for the STM32 ROM DFU device...")
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        if bootloader_is_present():
            print("STM32 ROM DFU device detected (VID:PID 0483:DF11).")
            return
        time.sleep(0.5)

    raise TimeoutError(
        "The STM32 ROM DFU device did not appear. Check that the MCU's native "
        "USB port is connected and that its system bootloader supports USB DFU."
    )


def main():
    parser = argparse.ArgumentParser(
        description="Send UPDATE over the STM32 USB CDC port and verify ROM DFU."
    )
    parser.add_argument(
        "--port",
        help="USB CDC COM port; normally detected automatically",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=15,
        help="Seconds to wait for ROM DFU after UPDATE (default: 15)",
    )
    args = parser.parse_args()

    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.error("pyserial is missing; install it with: py -m pip install pyserial")

    try:
        build_firmware()
        cdc_port = find_cdc_port(list_ports, args.port)
        if cdc_port is None:
            raise RuntimeError(
                "The STM32 USB CDC COM port is not present. ST-LINK USB is "
                "debug-only; connect the MCU's native USB port directly to this PC."
            )

        print("Opening {} and sending UPDATE...".format(cdc_port))
        with serial.Serial(
            cdc_port, 115200, timeout=3, write_timeout=2
        ) as port:
            port.reset_input_buffer()
            port.write(b"UPDATE")
            port.flush()
            response = port.read_until(b"\n")

        if response != b"OK\r\n":
            raise RuntimeError(
                "The CDC port did not return the expected acknowledgement; "
                "received {!r}.".format(response)
            )
        print("CDC acknowledgement received.")

        wait_for_bootloader(args.timeout)
        print("PASS: UPDATE reached the STM32 and it jumped to ROM DFU.")
    except (
        OSError,
        RuntimeError,
        subprocess.CalledProcessError,
        subprocess.TimeoutExpired,
        serial.SerialException,
        TimeoutError,
    ) as error:
        print("USB serial/DFU test failed: {}".format(error), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
