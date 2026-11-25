from __future__ import annotations

import ctypes
import fcntl
import logging
import os
import struct
import threading
import time
from typing import Callable

logger = logging.getLogger("st3d.pn532")

# Bit-reversal lookup table (PN532 SPI is LSB-first; Raspberry Pi spidev is MSB-first)
_REV = bytes(int(f"{i:08b}"[::-1], 2) for i in range(256))

# PN532 SPI frame markers
_SPI_DATAWRITE = 0x01
_SPI_STATREAD = 0x02
_SPI_DATAREAD = 0x03
_SPI_READY = 0x01

_PREAMBLE = 0x00
_STARTCODE1 = 0x00
_STARTCODE2 = 0xFF
_POSTAMBLE = 0x00
_HOSTTOPN532 = 0xD4
_PN532TOHOST = 0xD5
_ACK = bytes([0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00])

_CMD_GETFIRMWAREVERSION = 0x02
_CMD_SAMCONFIGURATION = 0x14
_CMD_RFCONFIGURATION = 0x32
_CMD_INLISTPASSIVETARGET = 0x4A

# Linux spidev ioctls (64-bit / 32-bit standard)
_SPI_IOC_WR_MODE = 0x40016B01
_SPI_IOC_WR_BITS_PER_WORD = 0x40016B03
_SPI_IOC_WR_MAX_SPEED_HZ = 0x40046B04
_SPI_IOC_MESSAGE_1 = 0x40206B00  # sizeof(struct spi_ioc_transfer) == 32

# Linux GPIO character device v1 ioctls (/dev/gpiochip0)
_GPIO_GET_LINEHANDLE_IOCTL = 0xC16CB403  # sizeof(gpiohandle_request) == 364
_GPIOHANDLE_SET_LINE_VALUES_IOCTL = 0xC040B409  # sizeof(gpiohandle_data) == 64
_GPIOHANDLE_REQUEST_OUTPUT = 0x02


def reverse_bytes(data: bytes | bytearray) -> bytes:
    return bytes(_REV[b] for b in data)


def build_pn532_frame(payload: bytes) -> bytes:
    """Build a PN532 normal information frame (TFI + command payload)."""
    tfi_and_data = bytes([_HOSTTOPN532]) + payload
    length = len(tfi_and_data)
    lcs = (-length) & 0xFF
    dcs = (-sum(tfi_and_data)) & 0xFF
    return bytes([_PREAMBLE, _STARTCODE1, _STARTCODE2, length, lcs]) + tfi_and_data + bytes([dcs, _POSTAMBLE])


def parse_pn532_response(raw: bytes, expected_cmd: int) -> bytes | None:
    """Parse PN532 response frame from raw DATAREAD bytes (excluding leading status byte)."""
    # Find 0x00 0xFF start code
    idx = -1
    for i in range(len(raw) - 5):
        if raw[i] == 0x00 and raw[i + 1] == 0xFF:
            idx = i
            break
    if idx < 0:
        return None
    length = raw[idx + 2]
    lcs = raw[idx + 3]
    if ((length + lcs) & 0xFF) != 0:
        return None
    start = idx + 4
    end = start + length
    if end >= len(raw):
        return None
    body = raw[start:end]
    dcs = raw[end]
    if ((sum(body) + dcs) & 0xFF) != 0:
        return None
    if len(body) < 2 or body[0] != _PN532TOHOST or body[1] != (expected_cmd + 1):
        return None
    return body[2:]


class _GpioOutputPin:
    """Zero-dependency Linux /dev/gpiochipN single output line via v1 ioctl."""

    def __init__(self, chip_path: str, offset: int, initial: int = 1, label: str = "pn532-cs") -> None:
        self._chip_fd = os.open(chip_path, os.O_RDWR)
        # struct gpiohandle_request: 64*uint32 lineoffsets, uint32 flags, 64*uint8 default_values,
        # char[32] consumer_label, uint32 lines, int32 fd
        req = bytearray(364)
        struct.pack_into("<I", req, 0, offset)
        struct.pack_into("<I", req, 256, _GPIOHANDLE_REQUEST_OUTPUT)
        req[260] = 1 if initial else 0
        lbl = label.encode("ascii")[:31]
        req[324 : 324 + len(lbl)] = lbl
        struct.pack_into("<I", req, 356, 1)
        try:
            fcntl.ioctl(self._chip_fd, _GPIO_GET_LINEHANDLE_IOCTL, req)
            self._line_fd = struct.unpack_from("<i", req, 360)[0]
            if self._line_fd < 0:
                raise OSError(f"Invalid GPIO line fd for {chip_path}:{offset}")
        except Exception:
            os.close(self._chip_fd)
            raise

    def set(self, high: bool) -> None:
        buf = bytearray(64)
        buf[0] = 1 if high else 0
        fcntl.ioctl(self._line_fd, _GPIOHANDLE_SET_LINE_VALUES_IOCTL, buf)

    def close(self) -> None:
        try:
            os.close(self._line_fd)
        except OSError:
            pass
        try:
            os.close(self._chip_fd)
        except OSError:
            pass


class _SpiDevice:
    """Zero-dependency Linux /dev/spidevB.D wrapper via ioctl."""

    def __init__(self, bus: int, device: int, speed_hz: int = 250000, mode: int = 0) -> None:
        self._speed_hz = speed_hz
        self._fd = os.open(f"/dev/spidev{bus}.{device}", os.O_RDWR)
        try:
            fcntl.ioctl(self._fd, _SPI_IOC_WR_MODE, struct.pack("B", mode))
            fcntl.ioctl(self._fd, _SPI_IOC_WR_BITS_PER_WORD, struct.pack("B", 8))
            fcntl.ioctl(self._fd, _SPI_IOC_WR_MAX_SPEED_HZ, struct.pack("<I", speed_hz))
        except Exception:
            os.close(self._fd)
            raise

    def xfer(self, tx: bytes) -> bytes:
        n = len(tx)
        tx_buf = ctypes.create_string_buffer(tx, n)
        rx_buf = ctypes.create_string_buffer(n)
        # struct spi_ioc_transfer (32 bytes): tx_buf(u64), rx_buf(u64), len(u32), speed_hz(u32),
        # delay_usecs(u16), bits_per_word(u8), cs_change(u8), tx_nbits(u8), rx_nbits(u8), word_delay(u8), pad(u8)
        tr = struct.pack(
            "<QQIIHBBBBBB",
            ctypes.addressof(tx_buf),
            ctypes.addressof(rx_buf),
            n,
            self._speed_hz,
            0,
            8,
            0,
            0,
            0,
            0,
            0,
        )
        fcntl.ioctl(self._fd, _SPI_IOC_MESSAGE_1, tr)
        return rx_buf.raw

    def close(self) -> None:
        try:
            os.close(self._fd)
        except OSError:
            pass


class Pn532SpiReader:
    """Low-level PN532 SPI driver supporting optional software CS GPIO (e.g. GPIO5 / Pin 29)."""

    def __init__(
        self,
        *,
        spi_bus: int = 0,
        spi_device: int = 0,
        cs_gpio: int | None = 5,
        gpio_chip: str = "/dev/gpiochip0",
        speed_hz: int = 250000,
    ) -> None:
        self._spi = _SpiDevice(spi_bus, spi_device, speed_hz=speed_hz, mode=0)
        self._cs: _GpioOutputPin | None = None
        if cs_gpio is not None and cs_gpio >= 0:
            try:
                self._cs = _GpioOutputPin(gpio_chip, cs_gpio, initial=1)
            except Exception:
                self._spi.close()
                raise

    def close(self) -> None:
        if self._cs is not None:
            try:
                self._cs.set(True)
            except OSError:
                pass
            self._cs.close()
            self._cs = None
        self._spi.close()

    def _xfer_lsb(self, data: bytes, hold_cs_pre_s: float = 0.001) -> bytes:
        tx = reverse_bytes(data)
        if self._cs is not None:
            self._cs.set(False)
            if hold_cs_pre_s > 0:
                time.sleep(hold_cs_pre_s)
        try:
            rx = self._spi.xfer(tx)
        finally:
            if self._cs is not None:
                self._cs.set(True)
        return reverse_bytes(rx)

    def wakeup(self) -> None:
        self._xfer_lsb(bytes(16), hold_cs_pre_s=0.005)
        time.sleep(0.03)

    def _wait_ready(self, timeout_s: float = 0.5) -> bool:
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rx = self._xfer_lsb(bytes([_SPI_STATREAD, 0x00]), hold_cs_pre_s=0.0005)
            if len(rx) >= 2 and rx[1] == _SPI_READY:
                return True
            time.sleep(0.01)
        return False

    def call(self, cmd: int, params: bytes = b"", response_len: int = 32, timeout_s: float = 0.5) -> bytes | None:
        frame = bytes([_SPI_DATAWRITE]) + build_pn532_frame(bytes([cmd]) + params)
        self._xfer_lsb(frame, hold_cs_pre_s=0.002)

        if not self._wait_ready(timeout_s=min(0.25, timeout_s)):
            return None
        ack_raw = self._xfer_lsb(bytes([_SPI_DATAREAD]) + bytes(6), hold_cs_pre_s=0.0005)
        if ack_raw[1:7] != _ACK:
            return None

        if not self._wait_ready(timeout_s=timeout_s):
            return None
        resp_raw = self._xfer_lsb(bytes([_SPI_DATAREAD]) + bytes(response_len), hold_cs_pre_s=0.0005)
        return parse_pn532_response(resp_raw[1:], cmd)

    def get_firmware_version(self) -> tuple[int, int, int, int] | None:
        res = self.call(_CMD_GETFIRMWAREVERSION, response_len=16, timeout_s=0.5)
        if not res or len(res) < 4:
            return None
        return res[0], res[1], res[2], res[3]

    def sam_configuration(self) -> bool:
        # Normal mode (0x01), timeout 50ms*20 = 1s (0x14), use IRQ pin (0x01)
        res = self.call(_CMD_SAMCONFIGURATION, bytes([0x01, 0x14, 0x01]), response_len=12, timeout_s=0.5)
        if res is None:
            return False
        # Set MaxRetries: MxRtyATR=0xFF, MxRtyPSL=0x01, MxRtyPassiveActivation=0x0A (~150ms per poll)
        self.call(_CMD_RFCONFIGURATION, bytes([0x05, 0xFF, 0x01, 0x0A]), response_len=12, timeout_s=0.3)
        return True

    def read_passive_target_uid(self, timeout_s: float = 0.25) -> str | None:
        """Poll for a single ISO14443A 106kbps card and return uppercase hex UID (e.g. '04AABBCCDD01')."""
        res = self.call(_CMD_INLISTPASSIVETARGET, bytes([0x01, 0x00]), response_len=32, timeout_s=timeout_s)
        if not res or len(res) < 6:
            return None
        num_targets = res[0]
        if num_targets < 1:
            return None
        uid_len = res[5]
        if uid_len == 0 or uid_len > 10 or len(res) < 6 + uid_len:
            return None
        return res[6 : 6 + uid_len].hex().upper()


class Pn532Poller:
    """24/7 background RFID polling loop with card-removal debouncing & automatic SPI recovery."""

    def __init__(
        self,
        *,
        on_scan: Callable[[str], None],
        spi_bus: int = 0,
        spi_device: int = 0,
        cs_gpio: int | None = 5,
        gpio_chip: str = "/dev/gpiochip0",
        speed_hz: int = 250000,
    ) -> None:
        self._on_scan = on_scan
        self._spi_bus = spi_bus
        self._spi_device = spi_device
        self._cs_gpio = cs_gpio
        self._gpio_chip = gpio_chip
        self._speed_hz = speed_hz
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

        self.connected = False
        self.firmware: str | None = None
        self.last_error: str | None = None
        self.last_uid: str | None = None
        self.last_scan_ts: float | None = None

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="pn532-poller", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2.0)

    def status_dict(self) -> dict[str, object]:
        return {
            "connected": self.connected,
            "chip": "PN532" if self.connected else None,
            "firmware": self.firmware,
            "spi": f"spidev{self._spi_bus}.{self._spi_device}",
            "csGpio": self._cs_gpio,
            "lastScanTs": self.last_scan_ts,
            "lastError": self.last_error,
        }

    def _run(self) -> None:
        backoff = 1.0
        while not self._stop.is_set():
            reader: Pn532SpiReader | None = None
            try:
                reader = Pn532SpiReader(
                    spi_bus=self._spi_bus,
                    spi_device=self._spi_device,
                    cs_gpio=self._cs_gpio,
                    gpio_chip=self._gpio_chip,
                    speed_hz=self._speed_hz,
                )
                reader.wakeup()
                fw = reader.get_firmware_version()
                if not fw or fw[0] != 0x32:
                    raise RuntimeError(f"PN532 nicht erkannt (fw={fw})")
                if not reader.sam_configuration():
                    raise RuntimeError("PN532 SAMConfiguration fehlgeschlagen")

                self.connected = True
                self.firmware = f"{fw[1]}.{fw[2]}"
                self.last_error = None
                backoff = 1.0
                logger.info(
                    "PN532 NFC-Reader bereit (IC=0x%02X, FW=%s, spidev%d.%d, CS=GPIO%s)",
                    fw[0],
                    self.firmware,
                    self._spi_bus,
                    self._spi_device,
                    self._cs_gpio,
                )

                active_uid: str | None = None
                last_seen_ts = 0.0
                last_emit_ts = 0.0
                consecutive_io_errors = 0

                while not self._stop.is_set():
                    try:
                        uid = reader.read_passive_target_uid(timeout_s=0.22)
                        consecutive_io_errors = 0
                    except OSError as exc:
                        consecutive_io_errors += 1
                        if consecutive_io_errors >= 5:
                            raise RuntimeError(f"SPI I/O-Fehler: {exc}") from exc
                        time.sleep(0.2)
                        continue

                    now = time.monotonic()
                    if uid:
                        last_seen_ts = now
                        # Emit immediately on new card, or after 3.0s if user holds card continuously
                        if uid != active_uid or (now - last_emit_ts) >= 3.0:
                            active_uid = uid
                            last_emit_ts = now
                            self.last_uid = uid
                            self.last_scan_ts = time.time()
                            logger.info("NFC-Karte erkannt (Länge %d Byte)", len(uid) // 2)
                            try:
                                self._on_scan(uid)
                            except Exception as cb_exc:
                                logger.warning("RFID Callback-Fehler: %s", cb_exc)
                    else:
                        # Card removed for >= 0.8s -> ready for same card again
                        if active_uid is not None and (now - last_seen_ts) >= 0.8:
                            active_uid = None

                    self._stop.wait(0.12)

            except Exception as exc:
                self.connected = False
                self.last_error = str(exc)
                logger.warning("PN532 Poller getrennt (%s) — Retry in %.0fs", exc, backoff)
                self._stop.wait(backoff)
                backoff = min(10.0, backoff * 2.0)
            finally:
                if reader is not None:
                    reader.close()
