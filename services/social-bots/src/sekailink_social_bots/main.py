from __future__ import annotations

import uvicorn

from .config import load_config
from .control_api import create_app
from .logging_utils import configure_logging
from .service import BotService


def run() -> None:
    config = load_config()
    configure_logging(config.log_level)
    service = BotService.create(config)
    app = create_app(service)
    uvicorn.run(app, host=config.host, port=config.port)


if __name__ == "__main__":
    run()
