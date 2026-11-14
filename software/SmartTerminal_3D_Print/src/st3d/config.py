"""Load the terminal configuration (TOML)."""

from __future__ import annotations

import os
import tomllib
from dataclasses import dataclass
from pathlib import Path

SEARCH_PATHS = (
    Path("/etc/st3d/config.toml"),
    Path(__file__).resolve().parents[2] / "config.toml",
)

VALID_ROTATIONS = (0, 90, 180, 270)


@dataclass(frozen=True)
class Config:
    rotation: int = 0
    source: str = "defaults"


def load() -> Config:
    env_path = os.environ.get("ST3D_CONFIG")
    candidates = (Path(env_path),) if env_path else SEARCH_PATHS
    for path in candidates:
        if path.is_file():
            with path.open("rb") as handle:
                data = tomllib.load(handle)
            return _parse(data, str(path))
    return Config()


def _parse(data: dict, source: str) -> Config:
    rotation = int(data.get("display", {}).get("rotation", 0))
    if rotation not in VALID_ROTATIONS:
        raise ValueError(f"display.rotation must be one of {VALID_ROTATIONS}, got {rotation}")
    return Config(rotation=rotation, source=source)
