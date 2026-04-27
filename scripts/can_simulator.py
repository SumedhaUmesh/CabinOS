#!/usr/bin/env python3
"""
Publishes synthetic battery SoC CAN frames to SocketCAN interface.

Frame mapping:
  CAN ID 0x100
  data[0] = SoC percent (0-100)

Example (Linux):
  sudo modprobe vcan
  sudo ip link add dev vcan0 type vcan
  sudo ip link set up vcan0
  python3 scripts/can_simulator.py --channel vcan0
"""

import argparse
import time

import can


def main() -> None:
    parser = argparse.ArgumentParser(description="CabinOS CAN simulator")
    parser.add_argument("--channel", default="vcan0", help="SocketCAN channel")
    parser.add_argument("--period-ms", type=int, default=1000, help="Publish interval in milliseconds")
    parser.add_argument("--start-soc", type=int, default=80, help="Starting battery SoC")
    args = parser.parse_args()

    bus = can.interface.Bus(channel=args.channel, bustype="socketcan")
    soc = max(0, min(100, args.start_soc))
    direction = -1

    print(f"Publishing battery SoC on {args.channel} every {args.period_ms} ms")
    while True:
        msg = can.Message(arbitration_id=0x100, data=[soc, 0, 0, 0, 0, 0, 0, 0], is_extended_id=False)
        bus.send(msg)
        print(f"Sent CAN 0x100 SoC={soc}%")

        if soc <= 10:
            direction = 1
        elif soc >= 95:
            direction = -1
        soc += direction

        time.sleep(args.period_ms / 1000.0)


if __name__ == "__main__":
    main()
