#!/usr/bin/env python3
"""
Database migration script for SekaiLink prototype
Adds new tables required for lobby system
"""

import os
import sys
from sqlalchemy import create_engine, text

# Load environment variables
from dotenv import load_dotenv
load_dotenv()

DATABASE_URL = os.getenv('DATABASE_URL')

if not DATABASE_URL:
    print("❌ ERROR: DATABASE_URL not found in .env file")
    sys.exit(1)

print("🔧 SekaiLink Database Migration")
print(f"Database: {DATABASE_URL.split('@')[1] if '@' in DATABASE_URL else 'unknown'}")
print("-" * 60)

engine = create_engine(DATABASE_URL)

migrations = [
    # 1. Create lobby_settings table
    """
    CREATE TABLE IF NOT EXISTS lobby_settings (
        id SERIAL PRIMARY KEY,
        lobby_id INTEGER UNIQUE REFERENCES lobbies(id),
        max_players INTEGER DEFAULT 10,
        time_limit_hours INTEGER,
        sync_rules TEXT DEFAULT '',
        allow_multigame BOOLEAN DEFAULT TRUE,
        allow_broadcast BOOLEAN DEFAULT TRUE,
        blacklisted_games TEXT DEFAULT '[]'
    );
    """,

    # 2. Create lobby_players table
    """
    CREATE TABLE IF NOT EXISTS lobby_players (
        id SERIAL PRIMARY KEY,
        lobby_id INTEGER REFERENCES lobbies(id),
        user_id VARCHAR REFERENCES users(id),
        game VARCHAR(100),
        yaml_file_id INTEGER REFERENCES yaml_files(id),
        rom_file_id INTEGER REFERENCES rom_files(id),
        is_ready BOOLEAN DEFAULT FALSE,
        status VARCHAR(20) DEFAULT 'waiting',
        patch_url VARCHAR(500),
        joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        finished_at TIMESTAMP
    );
    """,

    # 3. Create chat_messages table
    """
    CREATE TABLE IF NOT EXISTS chat_messages (
        id SERIAL PRIMARY KEY,
        lobby_id INTEGER REFERENCES lobbies(id),
        user_id VARCHAR REFERENCES users(id),
        message VARCHAR(500),
        message_type VARCHAR(20) DEFAULT 'user',
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    );
    """,

    # 4. Add new columns to lobbies table
    """
    ALTER TABLE lobbies
    ADD COLUMN IF NOT EXISTS visibility VARCHAR(20) DEFAULT 'open';
    """,

    """
    ALTER TABLE lobbies
    ADD COLUMN IF NOT EXISTS server_port INTEGER;
    """,

    """
    ALTER TABLE lobbies
    ADD COLUMN IF NOT EXISTS started_at TIMESTAMP;
    """,

    """
    ALTER TABLE lobbies
    ADD COLUMN IF NOT EXISTS ended_at TIMESTAMP;
    """,

    # 5. Add new columns to rom_files table
    """
    ALTER TABLE rom_files
    ADD COLUMN IF NOT EXISTS file_path VARCHAR(500);
    """,

    """
    ALTER TABLE rom_files
    ADD COLUMN IF NOT EXISTS uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
    """,
]

try:
    with engine.connect() as conn:
        print("📊 Running migrations...\n")

        for i, migration in enumerate(migrations, 1):
            try:
                conn.execute(text(migration))
                conn.commit()

                # Extract table name from SQL for better logging
                if "CREATE TABLE" in migration:
                    table_name = migration.split("CREATE TABLE IF NOT EXISTS")[1].split("(")[0].strip()
                    print(f"✅ [{i}/{len(migrations)}] Created table: {table_name}")
                elif "ALTER TABLE" in migration:
                    parts = migration.split()
                    table_name = parts[2]
                    column_name = parts[6] if len(parts) > 6 else "column"
                    print(f"✅ [{i}/{len(migrations)}] Added {column_name} to {table_name}")
                else:
                    print(f"✅ [{i}/{len(migrations)}] Migration executed")

            except Exception as e:
                # Check if error is because column/table already exists
                if "already exists" in str(e):
                    print(f"⏭️  [{i}/{len(migrations)}] Already exists, skipping")
                else:
                    print(f"⚠️  [{i}/{len(migrations)}] Warning: {str(e)}")

        print("\n" + "-" * 60)
        print("📋 Verifying database schema...\n")

        # Verify tables exist
        result = conn.execute(text("""
            SELECT table_name
            FROM information_schema.tables
            WHERE table_schema = 'public'
            ORDER BY table_name;
        """))

        tables = [row[0] for row in result]

        expected_tables = [
            'users', 'yaml_files', 'rom_files', 'lobbies',
            'lobby_settings', 'lobby_players', 'chat_messages'
        ]

        print("Tables found:")
        for table in tables:
            status = "✅" if table in expected_tables else "ℹ️ "
            print(f"  {status} {table}")

        missing = set(expected_tables) - set(tables)
        if missing:
            print(f"\n⚠️  Missing tables: {', '.join(missing)}")
        else:
            print(f"\n🎉 All required tables present!")

        print("\n" + "=" * 60)
        print("✅ Migration completed successfully!")
        print("=" * 60)

except Exception as e:
    print(f"\n❌ Migration failed: {str(e)}")
    sys.exit(1)
