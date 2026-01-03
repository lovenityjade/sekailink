# SekaiLink

**Next-generation Archipelago Multiworld hosting platform**

Powered by [Archipelago](https://archipelago.gg) | Inspired by [racetime.gg](https://racetime.gg)

---

## 🎮 What is SekaiLink?

SekaiLink is a modern web platform for hosting Archipelago multiworld randomizer sessions. It replaces the chaos of Discord-based coordination with a streamlined, real-time experience for players and hosts.

### Key Features

- **Real-time Lobbies**: WebSocket-powered live updates
- **Automatic Generation**: Integrated Archipelago seed generation
- **YAML Management**: Create, edit, and manage your game configurations
- **ROM Vault**: Secure temporary ROM storage with SHA-1 verification
- **Discord Integration**: OAuth authentication and community features
- **User Ratings**: Community-driven rating and review system
- **Moderation Tools**: Comprehensive admin and moderator controls
- **Multi-game Support**: Support for 89+ Archipelago games

---

## 📦 Current Status

**Phase 8 of 9 COMPLETE** (78% overall)

✅ **Phase 1: Foundation & Translation** - Database models, state machine, English translation
✅ **Phase 2: Core Pages & Navigation** - 17 templates, CSS system, responsive design
✅ **Phase 3: Games API Implementation** - Game catalog, filtering, favorites
✅ **Phase 4: Complete API Integration** - Lobby system, YAML/ROM management, Discord OAuth
✅ **Phase 5: Real-time Enhancements** - WebSocket system, live updates, friend status
✅ **Phase 6: Timer & Time Limit System** - Live timers, automatic enforcement
✅ **Phase 7: Rating & Review System** - User ratings, reviews, moderation (Backend complete)
✅ **Phase 8: Moderation & Admin Tools** - Full moderation dashboard, admin controls
⏳ **Phase 9: Polish & Production** - Final testing, optimization, deployment

See [PROGRESS.md](PROGRESS.md) for detailed status.

---

## 🏗️ Architecture

- **Backend**: Flask + SQLAlchemy + PostgreSQL
- **Real-time**: Flask-SocketIO + Redis
- **Task Queue**: Celery (for seed generation)
- **Frontend**: Vanilla JavaScript (no framework)
- **Auth**: Discord OAuth2
- **Deployment**: Docker Compose

### Tech Stack

```
Flask 3.0           - Web framework
PostgreSQL 15       - Database
Redis 7             - Cache & message broker
Celery 5.3          - Background tasks
Socket.IO 5.3       - Real-time communication
Docker Compose      - Container orchestration
```

---

## 🚀 Quick Start

### Prerequisites

- Docker & Docker Compose
- Git
- Discord Application (for OAuth)

### Installation

```bash
# Clone repository
git clone https://github.com/lovenityjade/sekailink.git
cd sekailink

# Create .env file from template
cp .env.template .env

# Edit .env with your credentials
nano .env  # Add Discord OAuth credentials

# Start services
docker compose up -d

# Check logs
docker compose logs -f api

# Populate game catalog (optional)
docker compose exec api python populate_games.py
```

### Access

- **Web Interface**: http://localhost:7000
- **API**: http://localhost:7000/api
- **Dashboard**: http://localhost:7000/dashboard.html
- **Lobby**: http://localhost:7000/lobby.html?id=1

---

## 📁 Project Structure

```
sekailink/
├── backend/
│   ├── main.py              # Flask application
│   ├── tasks.py             # Celery background tasks
│   ├── models/              # Database models
│   │   ├── __init__.py      # All models
│   │   └── choices.py       # State machine
│   ├── populate_games.py    # Game catalog script
│   └── static/              # Static files & uploads
├── frontend/
│   ├── src/                 # HTML pages
│   └── static/              # CSS & JS (coming soon)
├── discord_bot/
│   └── bot.py               # Discord bot (placeholder)
├── archipelago_core/        # Archipelago submodule
├── racetime_reference/      # Racetime.gg reference code
├── docker-compose.yml       # Docker services
├── .env.template            # Environment variables template
├── CLAUDE.md                # Project blueprint
├── PROGRESS.md              # Implementation progress
└── README.md                # This file
```

---

## 🗄️ Database Schema

### Core Models (Existing)
- **User**: Discord OAuth, profiles, badges
- **YamlFile**: YAML configurations
- **RomFile**: ROM uploads (temporary)
- **Lobby**: Multiworld sessions
- **LobbySettings**: Lobby configuration
- **LobbyPlayer**: Player participation
- **ChatMessage**: Lobby chat

### New Models (Phase 1)
- **Game**: Archipelago game catalog
- **FavoriteGame**: User favorites
- **Friend**: Friend relationships & blacklist
- **Ban**: Ban system with appeals
- **Warning**: User warnings
- **UserRating**: User-to-user ratings
- **UserReview**: Reviews with moderation
- **ServerRating**: Auto-calculated ratings
- **TwitchConnection**: Twitch OAuth
- **CustomWorld**: Custom world management

---

## 🎯 Roadmap

### Phase 1: Foundation & Translation (✅ Complete)
- ✅ Database models & state machine
- ✅ English translation
- ✅ Code refactoring

### Phase 2: Core Pages & Navigation (✅ Complete)
- ✅ Base template & CSS system
- ✅ 17 page templates
- ✅ Responsive dark theme design

### Phase 3: Games API Implementation (✅ Complete)
- ✅ 82 games in catalog
- ✅ Game filtering & search
- ✅ Favorite games system

### Phase 4: Complete API Integration (✅ Complete)
- ✅ 60+ API endpoints
- ✅ Lobby management system
- ✅ YAML & ROM management
- ✅ Discord OAuth integration

### Phase 5: Real-time Enhancements (✅ Complete)
- ✅ WebSocket system standardized
- ✅ Live lobby updates
- ✅ Friend online/offline status
- ✅ Chat system

### Phase 6: Timer & Time Limit System (✅ Complete)
- ✅ Live timer display
- ✅ Time limit enforcement
- ✅ Automatic completion
- ✅ Background monitoring

### Phase 7: Rating & Review System (🔄 In Progress - 60%)
- ✅ User-to-user ratings (4 criteria)
- ✅ Server behavior ratings
- ✅ Review moderation system
- ⏳ Frontend UI pending

### Phase 8: Moderation & Admin Tools (✅ Complete)
- ✅ Moderation dashboard
- ✅ Admin dashboard
- ✅ Ban/warn/appeal system
- ✅ Docker management
- ✅ System monitoring

### Phase 9: Polish & Production (⏳ Next)
- ⏳ Error handling improvements
- ⏳ Performance optimization
- ⏳ Complete testing suite
- ⏳ Production deployment

---

## 🤝 Contributing

This is a personal project but contributions are welcome!

### Development Workflow

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### Code Style

- **Backend**: Follow PEP 8
- **Frontend**: Use consistent formatting
- **Comments**: English preferred, French OK
- **Commits**: Descriptive messages

---

## 📜 License

MIT License - See LICENSE file for details

---

## 🙏 Credits

### Powered By
- **[Archipelago](https://archipelago.gg)** - Multiworld randomizer platform
- **[racetime.gg](https://racetime.gg)** - Architectural inspiration

### Hosting
- **[Hostinger](https://www.hostinger.com)** - VPS hosting

### Development
- Created by [lovenityjade](https://github.com/lovenityjade)
- Built with Claude Code (Anthropic)

---

## 📞 Contact & Support

- **Discord**: https://discord.gg/XvvcBxrRsk
- **GitHub**: https://github.com/lovenityjade/sekailink
- **Email**: sekailink@themiareproject.com

### Important Notice

⚠️ **SekaiLink is not affiliated with official Archipelago.gg technical support.**

For SekaiLink issues, use our Discord server. For Archipelago issues, visit the official [Archipelago Discord](https://discord.gg/archipelago).

---

## 💰 Support the Project

SekaiLink is free to use! Donations help cover server costs.

- VPS hosting: ~$40/month
- Domain: ~$15/year
- Development time: Volunteer

*Donation options coming soon*

---

**Happy Multiworld Gaming!** 🎮✨
