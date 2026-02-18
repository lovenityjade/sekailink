from __future__ import annotations

import hmac

from fastapi import Header, HTTPException, status



def build_auth_dependency(control_api_key: str):
    async def _auth(x_sekailink_bot_key: str | None = Header(default=None)) -> None:
        if not control_api_key:
            raise HTTPException(
                status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                detail="control_api_key_missing",
            )
        if not x_sekailink_bot_key or not hmac.compare_digest(x_sekailink_bot_key, control_api_key):
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="unauthorized")

    return _auth
