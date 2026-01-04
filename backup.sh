#!/bin/bash
# SekaiLink Backup Script
# Automated database and file backups

DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/home/sekailink/backups"

echo "🔄 Starting SekaiLink backup at $(date)"

# Ensure backup directory exists
mkdir -p $BACKUP_DIR

# Backup PostgreSQL database
echo "📦 Backing up PostgreSQL database..."
docker exec sekailink_db pg_dump -U sekailink_user sekailink > \
  $BACKUP_DIR/sekailink_db_$DATE.sql

if [ $? -eq 0 ]; then
    echo "✅ Database backup complete: sekailink_db_$DATE.sql"
else
    echo "❌ Database backup failed!"
    exit 1
fi

# Backup important files
echo "📦 Backing up project files..."
tar -czf $BACKUP_DIR/sekailink_files_$DATE.tar.gz \
  --exclude='/home/sekailink/backups' \
  --exclude='/home/sekailink/archipelago_core' \
  --exclude='/home/sekailink/racetime_reference' \
  --exclude='/home/sekailink/__pycache__' \
  --exclude='/home/sekailink/.git' \
  /home/sekailink/backend \
  /home/sekailink/frontend \
  /home/sekailink/docker-compose.yml \
  /home/sekailink/.env \
  /home/sekailink/README.md \
  /home/sekailink/TODO.md \
  /home/sekailink/PROGRESS.md \
  /home/sekailink/CLAUDE.md

if [ $? -eq 0 ]; then
    echo "✅ File backup complete: sekailink_files_$DATE.tar.gz"
else
    echo "❌ File backup failed!"
    exit 1
fi

# Get backup sizes
DB_SIZE=$(du -h $BACKUP_DIR/sekailink_db_$DATE.sql | cut -f1)
FILE_SIZE=$(du -h $BACKUP_DIR/sekailink_files_$DATE.tar.gz | cut -f1)

echo "📊 Backup sizes:"
echo "   - Database: $DB_SIZE"
echo "   - Files: $FILE_SIZE"

# Keep only last 7 days of backups
echo "🧹 Cleaning up old backups (keeping last 7 days)..."
find $BACKUP_DIR -name "sekailink_db_*.sql" -mtime +7 -delete
find $BACKUP_DIR -name "sekailink_files_*.tar.gz" -mtime +7 -delete

# Count remaining backups
DB_COUNT=$(find $BACKUP_DIR -name "sekailink_db_*.sql" | wc -l)
FILE_COUNT=$(find $BACKUP_DIR -name "sekailink_files_*.tar.gz" | wc -l)

echo "✅ Backup complete! Current backups: $DB_COUNT database(s), $FILE_COUNT file backup(s)"
echo "📍 Location: $BACKUP_DIR"
echo "✅ Done at $(date)"
