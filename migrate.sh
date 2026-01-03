#!/bin/bash
# Quick database migration script for SekaiLink

echo "🚀 SekaiLink Database Migration"
echo "================================"
echo ""

# Check if running in Docker
if [ -f /.dockerenv ]; then
    echo "📦 Running inside Docker container"
    cd /app
    python3 migrate_db.py
else
    echo "🐳 Running migration via Docker..."

    # Check if docker-compose is available
    if command -v docker-compose &> /dev/null; then
        echo "Starting database if not running..."
        docker-compose up -d db

        echo "Waiting 5 seconds for database to be ready..."
        sleep 5

        echo "Copying migration script to container..."
        docker cp migrate_db.py sekailink_api:/app/migrate_db.py

        echo "Running migration..."
        docker-compose exec api python3 /app/migrate_db.py

        echo ""
        echo "✅ Migration complete!"
    else
        echo "❌ docker-compose not found"
        echo "Please install docker-compose or run this script inside the Docker container"
        exit 1
    fi
fi
