#!/usr/bin/env python3
"""Send synthetic MagLab scalar or vector survey frames over UDP.

Scalar protocol (24 '+'-separated fields, one newline-terminated datagram):
    UU + id + time + lon + lat + altitude + posAccuracy + grdClearance +
    scalar1 + scalar2 + fluxgateX/Y/Z + roll/pitch/yaw +
    satellites + barometer + angularX/Y/Z + accX/Y/Z

Vector protocol (25 '+'-separated fields, one newline-terminated datagram):
    UU + len + time + lon + lat + altitude + fixType + lidar +
    fg1X/Y/Z + fg2X/Y/Z + gyroX/Y/Z + accX/Y/Z + roll/pitch/yaw +
    count + CRC16

This is deliberately a standalone process. It only sends UDP; it does not
import QGC, open a serial link or control a vehicle.

Examples:
    # Scalar (default UDP port 14551)
    python3 tools/maglab/magnetic_survey_simulator.py \
        --protocol scalar --host 127.0.0.1 --port 14551 --rate 5

    # Vector (default UDP port 13551)
    python3 tools/maglab/magnetic_survey_simulator.py \
        --protocol vector --host 127.0.0.1 --port 13551 --rate 5
"""

from __future__ import annotations

import argparse
import math
import random
import signal
import socket
import sys
import time

STOP = False

SCALAR_DEFAULT_PORT = 14551
VECTOR_DEFAULT_PORT = 13551


def _handle_signal(signum: int, frame: object) -> None:
    del signum, frame
    global STOP
    STOP = True


class SitlTelemetry:
    """Read-only MAVLink telemetry used to drive the synthetic survey path."""

    def __init__(self, endpoint: str, source_system: int = 251):
        try:
            from pymavlink import mavutil
        except ImportError as exc:
            raise RuntimeError(
                "pymavlink is required for --mavlink; run with the ArduPilot .venv "
                "or install pymavlink"
            ) from exc

        self._mavutil = mavutil
        self._master = mavutil.mavlink_connection(
            endpoint, source_system=source_system
        )
        self.lat = math.nan
        self.lon = math.nan
        self.alt = math.nan
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0
        self.satellites = 0
        self.barometer = math.nan
        self.received = 0

    def update(self) -> None:
        for _ in range(32):
            msg = self._master.recv_match(blocking=False)
            if msg is None:
                break
            kind = msg.get_type()
            if kind == "GLOBAL_POSITION_INT":
                self.received += 1
                self.lat = msg.lat / 1e7
                self.lon = msg.lon / 1e7
                self.alt = msg.alt / 1000.0
            elif kind == "GPS_RAW_INT":
                self.satellites = msg.satellites_visible
            elif kind == "ATTITUDE":
                self.roll = math.degrees(msg.roll)
                self.pitch = math.degrees(msg.pitch)
                self.yaw = (math.degrees(msg.yaw) + 360.0) % 360.0
            elif kind == "VFR_HUD":
                if not math.isfinite(self.alt):
                    self.alt = msg.alt
                self.barometer = msg.alt
                self.yaw = (msg.heading + 360.0) % 360.0

    @property
    def valid(self) -> bool:
        return (
            math.isfinite(self.lat)
            and math.isfinite(self.lon)
            and -90.0 < self.lat < 90.0
            and -180.0 <= self.lon <= 180.0
        )


# Keep pymavlink optional for the default synthetic mode.
if False:  # pragma: no cover - marks the optional import shape for readers
    from pymavlink import mavutil  # type: ignore


def _sample_state(
    *,
    elapsed: float,
    base_lat: float,
    base_lon: float,
    radius_m: float,
    altitude_m: float,
    angle: float,
    rng: random.Random,
    telemetry: SitlTelemetry | None = None,
) -> tuple[float, float, float, float, float, float, int, float, float]:
    """Return position, attitude, satellite count, barometer and anomaly."""
    if telemetry is not None and telemetry.valid:
        lat = telemetry.lat
        lon = telemetry.lon
        altitude_m = telemetry.alt if math.isfinite(telemetry.alt) else altitude_m
        roll = telemetry.roll
        pitch = telemetry.pitch
        yaw = telemetry.yaw
        satellites = telemetry.satellites if telemetry.satellites > 0 else 18
        barometer = telemetry.barometer if math.isfinite(telemetry.barometer) else altitude_m
    else:
        lat = base_lat + radius_m * math.sin(angle) / 111_320.0
        lon = base_lon + radius_m * math.cos(angle) / (
            111_320.0 * max(0.01, math.cos(math.radians(base_lat)))
        )
        roll = 0.0
        pitch = 0.0
        yaw = (math.degrees(angle) + 90.0) % 360.0
        satellites = 18
        barometer = altitude_m + 0.2

    # Keep the synthetic values finite and inside the protocol receiver's bounds.
    lat = max(-89.9, min(89.9, lat))
    lon = max(-179.9, min(179.9, lon))

    # A smooth anomaly plus tiny measurement noise. The magnetometer values
    # remain realistic; their difference is the mapped quantity.
    anomaly = 22.0 * math.sin(angle * 2.0) + 8.0 * math.sin(angle * 5.0)
    del elapsed, rng  # reserved for future time-correlated noise models
    return lat, lon, altitude_m, roll, pitch, yaw, satellites, barometer, anomaly


def scalar_values(
    *,
    sample_id: int,
    elapsed: float,
    base_lat: float,
    base_lon: float,
    radius_m: float,
    altitude_m: float,
    angle: float,
    rng: random.Random,
    telemetry: SitlTelemetry | None = None,
) -> list[str]:
    lat, lon, altitude_m, roll, pitch, yaw, satellites, barometer, anomaly = _sample_state(
        elapsed=elapsed,
        base_lat=base_lat,
        base_lon=base_lon,
        radius_m=radius_m,
        altitude_m=altitude_m,
        angle=angle,
        rng=rng,
        telemetry=telemetry,
    )
    s1 = 50_000.0 + anomaly + rng.uniform(-0.8, 0.8)
    s2 = 49_500.0 + 0.6 * anomaly + rng.uniform(-0.8, 0.8)
    return [
        "UU",
        str(sample_id),
        f"{elapsed:.3f}",
        f"{lon:.7f}",
        f"{lat:.7f}",
        f"{altitude_m:.3f}",
        "0.500",
        "2.000",
        f"{s1:.3f}",
        f"{s2:.3f}",
        "0.100",
        "-0.050",
        "0.020",
        f"{roll:.3f}",
        f"{pitch:.3f}",
        f"{yaw:.3f}",
        str(satellites),
        f"{barometer:.3f}",
        "0.001",
        "0.002",
        "0.003",
        "0.010",
        "0.020",
        "9.810",
    ]


def vector_values(
    *,
    sample_id: int,
    elapsed: float,
    base_lat: float,
    base_lon: float,
    radius_m: float,
    altitude_m: float,
    angle: float,
    rng: random.Random,
    telemetry: SitlTelemetry | None = None,
) -> list[str]:
    lat, lon, altitude_m, roll, pitch, yaw, satellites, barometer, anomaly = _sample_state(
        elapsed=elapsed,
        base_lat=base_lat,
        base_lon=base_lon,
        radius_m=radius_m,
        altitude_m=altitude_m,
        angle=angle,
        rng=rng,
        telemetry=telemetry,
    )
    del satellites, barometer  # not part of the vector frame

    # Build two vector magnetometer readings from a common unit direction, so
    # the receiver can preserve raw XYZ and still reconstruct |B1|-|B2|.
    direction = (math.sin(angle), math.cos(angle), 0.55)
    magnitude = math.sqrt(sum(value * value for value in direction))
    ux, uy, uz = (value / magnitude for value in direction)

    b1 = 50_000.0 + anomaly
    b2 = 49_500.0 + 0.6 * anomaly
    fg1 = [b1 * ux + rng.uniform(-0.5, 0.5),
           b1 * uy + rng.uniform(-0.5, 0.5),
           b1 * uz + rng.uniform(-0.5, 0.5)]
    fg2 = [b2 * ux + rng.uniform(-0.5, 0.5),
           b2 * uy + rng.uniform(-0.5, 0.5),
           b2 * uz + rng.uniform(-0.5, 0.5)]

    values = [
        "UU",
        "0",  # length placeholder, filled after all fields exist
        f"{elapsed:.3f}",
        f"{lon:.7f}",
        f"{lat:.7f}",
        f"{altitude_m:.3f}",
        "3",
        f"{max(0.0, altitude_m - 2.0):.3f}",
        *(f"{value:.3f}" for value in fg1),
        *(f"{value:.3f}" for value in fg2),
        "0.001", "0.002", "0.003",
        "0.010", "0.020", "9.810",
        f"{roll:.3f}", f"{pitch:.3f}", f"{yaw:.3f}",
        str(sample_id),
    ]

    # len is informational only in the current QGC parser. Use the payload
    # length so captures look like the documented wire format.
    values[1] = str(len("+".join(values[2:])))
    values.append(f"{_crc16_ccitt('+'.join(values).encode('ascii')):04X}")
    return values


def _crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def make_frame(values: list[str], protocol: str = "scalar") -> bytes:
    expected = 24 if protocol == "scalar" else 25
    if len(values) != expected:
        raise ValueError(f"a {protocol} frame must contain {expected} fields")
    if not values or values[0] != "UU":
        raise ValueError("frame must start with UU")
    return ("+".join(values) + "\n").encode("ascii")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send synthetic MagLab scalar or vector survey frames over UDP"
    )
    parser.add_argument(
        "--protocol",
        choices=("scalar", "vector"),
        default="scalar",
        help="survey protocol to simulate (default: scalar)",
    )
    parser.add_argument("--host", default="127.0.0.1", help="QGC listener host")
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        help="QGC UDP port (default: 14551 scalar, 13551 vector)",
    )
    parser.add_argument("--rate", type=float, default=5.0, help="frames per second")
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="seconds to run; 0 means until Ctrl+C",
    )
    parser.add_argument("--lat", type=float, default=36.0, help="path centre latitude")
    parser.add_argument("--lon", type=float, default=120.0, help="path centre longitude")
    parser.add_argument("--radius", type=float, default=30.0, help="path radius in metres")
    parser.add_argument("--altitude", type=float, default=50.0, help="altitude in metres")
    parser.add_argument("--seed", type=int, default=1, help="noise seed")
    parser.add_argument(
        "--mavlink",
        default="",
        help="read-only SITL MAVLink endpoint, e.g. udp:127.0.0.1:14552",
    )
    parser.add_argument("--quiet", action="store_true", help="only print errors")
    args = parser.parse_args()

    if args.port is None:
        args.port = SCALAR_DEFAULT_PORT if args.protocol == "scalar" else VECTOR_DEFAULT_PORT
    if not 0.1 <= args.rate <= 200.0:
        parser.error("--rate must be between 0.1 and 200")
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if args.radius < 0:
        parser.error("--radius must be non-negative")
    if not (-90.0 < args.lat < 90.0 and -180.0 <= args.lon <= 180.0):
        parser.error("--lat/--lon outside valid range")

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    rng = random.Random(args.seed)
    telemetry = None
    if args.mavlink:
        try:
            telemetry = SitlTelemetry(args.mavlink)
        except RuntimeError as exc:
            print(str(exc), file=sys.stderr)
            return 1
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    destination = (args.host, args.port)
    start = time.monotonic()
    next_send = start
    sent = 0
    interval = 1.0 / args.rate
    sample_id = 1

    if not args.quiet:
        print(
            f"sending UU {args.protocol} frames to {destination[0]}:{destination[1]} "
            f"at {args.rate:g} Hz; Ctrl+C to stop"
        )

    try:
        while not STOP:
            now = time.monotonic()
            if args.duration > 0 and now - start >= args.duration:
                break
            if now < next_send:
                time.sleep(min(next_send - now, 0.05))
                continue

            elapsed = now - start
            angle = elapsed * 0.08
            if telemetry is not None:
                telemetry.update()
                if telemetry.received and telemetry.valid:
                    base_lat = telemetry.lat
                    base_lon = telemetry.lon
            values_fn = scalar_values if args.protocol == "scalar" else vector_values
            values = values_fn(
                sample_id=sample_id,
                elapsed=elapsed,
                base_lat=args.lat,
                base_lon=args.lon,
                radius_m=args.radius,
                altitude_m=args.altitude,
                angle=angle,
                rng=rng,
                telemetry=telemetry,
            )
            try:
                sock.sendto(make_frame(values, args.protocol), destination)
            except OSError as exc:
                print(f"send failed: {exc}", file=sys.stderr)
                return 1

            sent += 1
            sample_id += 1
            next_send += interval
            # If the scheduler falls behind, skip stale slots instead of
            # flooding the receiver with a burst.
            if next_send < now - interval:
                next_send = now + interval
    finally:
        sock.close()

    if not args.quiet:
        print(f"stopped after {sent} {args.protocol} frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
