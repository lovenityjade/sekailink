#!/usr/bin/env python3
"""Sign SekaiLink release entries with the offline Ed25519 release key."""

from __future__ import annotations

import argparse
import base64
import json
import pathlib
import subprocess
import tempfile


KEY_ID = "sekailink-release-ed25519-2026-01"


def signature_payload(release: dict) -> bytes:
    fields = {
        "version": str(release.get("version") or release.get("latest") or ""),
        "channel": str(release.get("channel") or ""),
        "platform": str(release.get("platform") or ""),
        "build": str(release.get("build") or "release"),
        "artifact_type": str(release.get("artifact_type") or ""),
        "download_url": str(release.get("download_url") or release.get("url") or ""),
        "sha256": str(release.get("sha256") or "").lower(),
        "release_sequence": str(release.get("release_sequence") or ""),
    }
    if not all(fields.values()):
        missing = ",".join(key for key, value in fields.items() if not value)
        raise ValueError(f"release entry is incomplete for signing: {missing}")
    body = "sekailink-release-v1\n" + "".join(f"{key}={value}\n" for key, value in fields.items())
    return body.encode("utf-8")


def sign(payload: bytes, private_key: pathlib.Path) -> str:
    with tempfile.NamedTemporaryFile(prefix="sekailink-release-payload-") as payload_file:
        payload_file.write(payload)
        payload_file.flush()
        result = subprocess.run(
            [
                "openssl",
                "pkeyutl",
                "-sign",
                "-rawin",
                "-inkey",
                str(private_key),
                "-in",
                payload_file.name,
            ],
            capture_output=True,
        )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"OpenSSL could not sign the release manifest: {detail}")
    if len(result.stdout) != 64:
        raise RuntimeError("unexpected Ed25519 signature length")
    return base64.b64encode(result.stdout).decode("ascii")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=pathlib.Path)
    parser.add_argument("--private-key", required=True, type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument(
        "--release-sequence",
        type=int,
        help="Monotonically increasing release number applied to every signed entry",
    )
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    releases = manifest.get("releases")
    if not isinstance(releases, list) or not releases:
        raise ValueError("manifest has no release entries")
    for release in releases:
        if not isinstance(release, dict):
            raise ValueError("release entry must be an object")
        if args.release_sequence is not None:
            release["release_sequence"] = args.release_sequence
        sequence = release.get("release_sequence")
        if not isinstance(sequence, int) or isinstance(sequence, bool) or sequence <= 0:
            raise ValueError("release_sequence must be a positive integer")
        release["signature"] = sign(signature_payload(release), args.private_key)
        release["signing_key_id"] = KEY_ID

    output = args.output or args.manifest
    temp = output.with_suffix(output.suffix + ".tmp")
    temp.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    temp.replace(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
