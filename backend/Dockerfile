FROM python:3.11-slim
WORKDIR /app
RUN apt-get update && apt-get install -y git build-essential libpq-dev p7zip-full && rm -rf /var/lib/apt-cache
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
# Use eventlet worker for WebSocket support
# Note: Using single worker with eventlet for SocketIO (multiple workers require message queue)
CMD ["gunicorn", "--bind", "0.0.0.0:7000", "--workers", "1", "--worker-class", "eventlet", "--timeout", "120", "main:app"]
