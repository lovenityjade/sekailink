from __future__ import annotations

import logging
import re


_SECRET_PATTERNS = [
    re.compile(r"(token=)([^\s&]+)", re.IGNORECASE),
    re.compile(r"(authorization:\s*bearer\s+)(\S+)", re.IGNORECASE),
    re.compile(r"(x-sekailink-bot-key:\s*)(\S+)", re.IGNORECASE),
]


class SecretScrubFilter(logging.Filter):
    def filter(self, record: logging.LogRecord) -> bool:
        message = str(record.getMessage())
        for pattern in _SECRET_PATTERNS:
            message = pattern.sub(r"\1[REDACTED]", message)
        record.msg = message
        record.args = ()
        return True


def configure_logging(level: str = "INFO") -> None:
    logging.basicConfig(
        level=getattr(logging, level.upper(), logging.INFO),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )
    root_logger = logging.getLogger()
    root_logger.addFilter(SecretScrubFilter())
