"""
State Machine Choices (Racetime.gg Pattern)

Defines all valid states and transitions for lobbies and players.
"""

class LobbyStates:
    """Valid lobby states"""
    OPEN = 'open'  # Anyone can join
    PENDING = 'pending'  # Some players ready, waiting for all
    GENERATING = 'generating'  # Seed being generated
    READY = 'ready'  # Generation complete, waiting to start
    ACTIVE = 'active'  # Sync in progress
    FINISHED = 'finished'  # Completed successfully
    CANCELLED = 'cancelled'  # Host cancelled
    FAILED = 'failed'  # Generation or server error

    ALL = [OPEN, PENDING, GENERATING, READY, ACTIVE, FINISHED, CANCELLED, FAILED]
    ACTIVE_STATES = [OPEN, PENDING, GENERATING, READY, ACTIVE]
    COMPLETED_STATES = [FINISHED, CANCELLED, FAILED]


class PlayerStates:
    """Valid player states"""
    WAITING = 'waiting'  # Joined, not ready
    READY = 'ready'  # YAML/ROM uploaded, ready to generate
    ACTIVE = 'active'  # Playing
    FINISHED = 'finished'  # Completed successfully
    DNF = 'dnf'  # Did not finish (quit/disconnect)
    FORFEIT = 'forfeit'  # Gave up intentionally

    ALL = [WAITING, READY, ACTIVE, FINISHED, DNF, FORFEIT]
    ACTIVE_STATES = [WAITING, READY, ACTIVE]
    COMPLETED_STATES = [FINISHED, DNF, FORFEIT]


class LobbyVisibility:
    """Lobby visibility options"""
    OPEN = 'open'  # Public, anyone can join
    PRIVATE = 'private'  # Requires invite/password
    CLOSED = 'closed'  # No new joins allowed

    ALL = [OPEN, PRIVATE, CLOSED]


class FriendStatus:
    """Friend relationship status"""
    PENDING = 'pending'  # Friend request sent
    ACCEPTED = 'accepted'  # Friends
    BLOCKED = 'blocked'  # Blacklisted

    ALL = [PENDING, ACCEPTED, BLOCKED]


class BanAppealStatus:
    """Ban appeal status"""
    PENDING = 'pending'  # Appeal submitted
    APPROVED = 'approved'  # Appeal accepted, ban lifted
    REJECTED = 'rejected'  # Appeal denied

    ALL = [PENDING, APPROVED, REJECTED]


class ReviewStatus:
    """User review moderation status"""
    PENDING = 'pending'  # Awaiting moderation
    APPROVED = 'approved'  # Approved by moderator
    REJECTED = 'rejected'  # Rejected by moderator

    ALL = [PENDING, APPROVED, REJECTED]


class CustomWorldStatus:
    """Custom world approval status"""
    PENDING = 'pending'  # Awaiting admin review
    APPROVED = 'approved'  # Approved for use
    REJECTED = 'rejected'  # Rejected/unsafe

    ALL = [PENDING, APPROVED, REJECTED]


class UserRoles:
    """User permission roles"""
    USER = 'user'  # Regular user
    MODERATOR = 'moderator'  # Can moderate lobbies/users
    ADMIN = 'admin'  # Full system access

    ALL = [USER, MODERATOR, ADMIN]


class MessageTypes:
    """Chat message types"""
    USER = 'user'  # User message
    SYSTEM = 'system'  # System announcement
    BOT = 'bot'  # Bot message

    ALL = [USER, SYSTEM, BOT]


# ============================================================================
# STATE TRANSITION VALIDATION
# ============================================================================

LOBBY_TRANSITIONS = {
    # From OPEN
    LobbyStates.OPEN: [
        LobbyStates.PENDING,  # Players start readying up
        LobbyStates.GENERATING,  # Direct to generation if all ready
        LobbyStates.CANCELLED,  # Host cancels
    ],

    # From PENDING
    LobbyStates.PENDING: [
        LobbyStates.OPEN,  # Player unreadies
        LobbyStates.GENERATING,  # All players ready
        LobbyStates.CANCELLED,  # Host cancels
    ],

    # From GENERATING
    LobbyStates.GENERATING: [
        LobbyStates.READY,  # Generation successful
        LobbyStates.FAILED,  # Generation error
        LobbyStates.CANCELLED,  # Host cancels during generation
    ],

    # From READY
    LobbyStates.READY: [
        LobbyStates.ACTIVE,  # Host starts sync
        LobbyStates.CANCELLED,  # Host cancels
    ],

    # From ACTIVE
    LobbyStates.ACTIVE: [
        LobbyStates.FINISHED,  # All players finish or time limit reached
        LobbyStates.CANCELLED,  # Host force stops
    ],

    # Terminal states (no transitions)
    LobbyStates.FINISHED: [],
    LobbyStates.CANCELLED: [],
    LobbyStates.FAILED: [],
}


PLAYER_TRANSITIONS = {
    # From WAITING
    PlayerStates.WAITING: [
        PlayerStates.READY,  # Uploads YAML/ROM
        PlayerStates.DNF,  # Leaves before starting
    ],

    # From READY
    PlayerStates.READY: [
        PlayerStates.WAITING,  # Unreadies
        PlayerStates.ACTIVE,  # Sync starts
        PlayerStates.DNF,  # Leaves
    ],

    # From ACTIVE
    PlayerStates.ACTIVE: [
        PlayerStates.FINISHED,  # Completes
        PlayerStates.DNF,  # Disconnects/quits
        PlayerStates.FORFEIT,  # Intentionally gives up
    ],

    # Terminal states
    PlayerStates.FINISHED: [],
    PlayerStates.DNF: [],
    PlayerStates.FORFEIT: [],
}


def can_transition_lobby(from_state, to_state):
    """Check if lobby state transition is valid"""
    if from_state not in LOBBY_TRANSITIONS:
        return False
    return to_state in LOBBY_TRANSITIONS[from_state]


def can_transition_player(from_state, to_state):
    """Check if player state transition is valid"""
    if from_state not in PLAYER_TRANSITIONS:
        return False
    return to_state in PLAYER_TRANSITIONS[from_state]
