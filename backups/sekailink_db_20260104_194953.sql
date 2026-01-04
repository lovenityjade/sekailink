--
-- PostgreSQL database dump
--

\restrict 4dqMmFOlYAXApa2NaVwfVztuaofoCpixlMOV9ZPZFH1dWa5BhSpSCNwgxwzWGvy

-- Dumped from database version 15.15
-- Dumped by pg_dump version 15.15

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: bans; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.bans (
    id integer NOT NULL,
    user_id character varying,
    banned_by character varying,
    reason text NOT NULL,
    duration_hours integer,
    appeal_text text,
    appeal_status character varying(20),
    appeal_reviewed_by character varying,
    appeal_reviewed_at timestamp without time zone,
    created_at timestamp without time zone,
    expires_at timestamp without time zone,
    is_active boolean
);


ALTER TABLE public.bans OWNER TO sekailink_user;

--
-- Name: bans_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.bans_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.bans_id_seq OWNER TO sekailink_user;

--
-- Name: bans_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.bans_id_seq OWNED BY public.bans.id;


--
-- Name: chat_messages; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.chat_messages (
    id integer NOT NULL,
    lobby_id integer,
    user_id character varying,
    message character varying(500),
    message_type character varying(20),
    is_pinned boolean,
    pinned_by character varying,
    deleted boolean,
    deleted_by character varying,
    deleted_at timestamp without time zone,
    created_at timestamp without time zone
);


ALTER TABLE public.chat_messages OWNER TO sekailink_user;

--
-- Name: chat_messages_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.chat_messages_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.chat_messages_id_seq OWNER TO sekailink_user;

--
-- Name: chat_messages_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.chat_messages_id_seq OWNED BY public.chat_messages.id;


--
-- Name: custom_worlds; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.custom_worlds (
    id integer NOT NULL,
    name character varying(200) NOT NULL,
    slug character varying(100) NOT NULL,
    version character varying(50),
    file_path character varying(500) NOT NULL,
    uploader_id character varying,
    status character varying(20),
    reviewed_by character varying,
    reviewed_at timestamp without time zone,
    rejection_reason text,
    created_at timestamp without time zone
);


ALTER TABLE public.custom_worlds OWNER TO sekailink_user;

--
-- Name: custom_worlds_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.custom_worlds_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.custom_worlds_id_seq OWNER TO sekailink_user;

--
-- Name: custom_worlds_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.custom_worlds_id_seq OWNED BY public.custom_worlds.id;


--
-- Name: favorite_games; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.favorite_games (
    id integer NOT NULL,
    user_id character varying,
    game_slug character varying(100),
    created_at timestamp without time zone
);


ALTER TABLE public.favorite_games OWNER TO sekailink_user;

--
-- Name: favorite_games_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.favorite_games_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.favorite_games_id_seq OWNER TO sekailink_user;

--
-- Name: favorite_games_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.favorite_games_id_seq OWNED BY public.favorite_games.id;


--
-- Name: friends; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.friends (
    id integer NOT NULL,
    user_id character varying,
    friend_id character varying,
    status character varying(20),
    created_at timestamp without time zone
);


ALTER TABLE public.friends OWNER TO sekailink_user;

--
-- Name: friends_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.friends_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.friends_id_seq OWNER TO sekailink_user;

--
-- Name: friends_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.friends_id_seq OWNED BY public.friends.id;


--
-- Name: games; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.games (
    id integer NOT NULL,
    slug character varying(100) NOT NULL,
    name character varying(200) NOT NULL,
    description text,
    boxart_url character varying(500),
    requires_rom boolean,
    world_type character varying(20),
    sync_count integer,
    is_active boolean,
    created_at timestamp without time zone
);


ALTER TABLE public.games OWNER TO sekailink_user;

--
-- Name: games_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.games_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.games_id_seq OWNER TO sekailink_user;

--
-- Name: games_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.games_id_seq OWNED BY public.games.id;


--
-- Name: lobbies; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.lobbies (
    id integer NOT NULL,
    host_id character varying,
    name character varying(100),
    slug character varying(100),
    status character varying(20),
    visibility character varying(20),
    seed_url character varying(255),
    server_port integer,
    time_limit_hours integer,
    restrict_time_limit boolean,
    timer_started_at timestamp without time zone,
    created_at timestamp without time zone,
    started_at timestamp without time zone,
    ended_at timestamp without time zone
);


ALTER TABLE public.lobbies OWNER TO sekailink_user;

--
-- Name: lobbies_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.lobbies_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.lobbies_id_seq OWNER TO sekailink_user;

--
-- Name: lobbies_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.lobbies_id_seq OWNED BY public.lobbies.id;


--
-- Name: lobby_players; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.lobby_players (
    id integer NOT NULL,
    lobby_id integer,
    user_id character varying,
    game character varying(100),
    yaml_file_id integer,
    rom_file_id integer,
    is_ready boolean,
    status character varying(20),
    patch_url character varying(500),
    joined_at timestamp without time zone,
    finished_at timestamp without time zone
);


ALTER TABLE public.lobby_players OWNER TO sekailink_user;

--
-- Name: lobby_players_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.lobby_players_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.lobby_players_id_seq OWNER TO sekailink_user;

--
-- Name: lobby_players_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.lobby_players_id_seq OWNED BY public.lobby_players.id;


--
-- Name: lobby_settings; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.lobby_settings (
    id integer NOT NULL,
    lobby_id integer,
    max_players integer,
    time_limit_hours integer,
    sync_rules text,
    allow_multigame boolean,
    allow_broadcast boolean,
    disallow_rom_games boolean,
    disallow_custom_worlds boolean,
    voice_chat_enabled boolean,
    blacklisted_games text
);


ALTER TABLE public.lobby_settings OWNER TO sekailink_user;

--
-- Name: lobby_settings_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.lobby_settings_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.lobby_settings_id_seq OWNER TO sekailink_user;

--
-- Name: lobby_settings_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.lobby_settings_id_seq OWNED BY public.lobby_settings.id;


--
-- Name: rom_files; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.rom_files (
    id integer NOT NULL,
    user_id character varying,
    filename character varying(255),
    file_path character varying(500),
    sha1 character varying(40),
    game_detected character varying(100),
    status character varying(20),
    uploaded_at timestamp without time zone,
    lobby_id integer
);


ALTER TABLE public.rom_files OWNER TO sekailink_user;

--
-- Name: rom_files_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.rom_files_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.rom_files_id_seq OWNER TO sekailink_user;

--
-- Name: rom_files_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.rom_files_id_seq OWNED BY public.rom_files.id;


--
-- Name: server_ratings; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.server_ratings (
    id integer NOT NULL,
    user_id character varying,
    kicks_count integer,
    bans_count integer,
    suspensions_count integer,
    syncs_finished integer,
    syncs_ragequit integer,
    rating double precision,
    updated_at timestamp without time zone
);


ALTER TABLE public.server_ratings OWNER TO sekailink_user;

--
-- Name: server_ratings_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.server_ratings_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.server_ratings_id_seq OWNER TO sekailink_user;

--
-- Name: server_ratings_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.server_ratings_id_seq OWNED BY public.server_ratings.id;


--
-- Name: twitch_connections; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.twitch_connections (
    id integer NOT NULL,
    user_id character varying,
    twitch_id character varying NOT NULL,
    twitch_username character varying NOT NULL,
    access_token character varying(500) NOT NULL,
    refresh_token character varying(500) NOT NULL,
    token_expires_at timestamp without time zone NOT NULL,
    created_at timestamp without time zone,
    updated_at timestamp without time zone
);


ALTER TABLE public.twitch_connections OWNER TO sekailink_user;

--
-- Name: twitch_connections_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.twitch_connections_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.twitch_connections_id_seq OWNER TO sekailink_user;

--
-- Name: twitch_connections_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.twitch_connections_id_seq OWNED BY public.twitch_connections.id;


--
-- Name: user_ratings; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.user_ratings (
    id integer NOT NULL,
    from_user_id character varying,
    to_user_id character varying,
    punctuality integer,
    respect_others integer,
    respect_rules integer,
    valid_release integer,
    overall_rating double precision,
    review_text text,
    created_at timestamp without time zone
);


ALTER TABLE public.user_ratings OWNER TO sekailink_user;

--
-- Name: user_ratings_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.user_ratings_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.user_ratings_id_seq OWNER TO sekailink_user;

--
-- Name: user_ratings_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.user_ratings_id_seq OWNED BY public.user_ratings.id;


--
-- Name: user_reviews; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.user_reviews (
    id integer NOT NULL,
    from_user_id character varying,
    to_user_id character varying,
    review_text text NOT NULL,
    likes integer,
    dislikes integer,
    reports integer,
    status character varying(20),
    moderated_by character varying,
    moderated_at timestamp without time zone,
    created_at timestamp without time zone
);


ALTER TABLE public.user_reviews OWNER TO sekailink_user;

--
-- Name: user_reviews_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.user_reviews_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.user_reviews_id_seq OWNER TO sekailink_user;

--
-- Name: user_reviews_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.user_reviews_id_seq OWNED BY public.user_reviews.id;


--
-- Name: users; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.users (
    id character varying NOT NULL,
    discord_id character varying NOT NULL,
    username character varying NOT NULL,
    avatar_url character varying,
    email character varying,
    email_verified boolean,
    pronouns character varying(50),
    bio text,
    language character varying(5),
    role character varying(20),
    is_banned boolean,
    is_suspended boolean,
    last_seen timestamp without time zone,
    badge_supporter boolean,
    badge_host_master boolean,
    badge_sync_gamer boolean,
    created_at timestamp without time zone
);


ALTER TABLE public.users OWNER TO sekailink_user;

--
-- Name: warnings; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.warnings (
    id integer NOT NULL,
    user_id character varying,
    warned_by character varying,
    reason text NOT NULL,
    created_at timestamp without time zone
);


ALTER TABLE public.warnings OWNER TO sekailink_user;

--
-- Name: warnings_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.warnings_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.warnings_id_seq OWNER TO sekailink_user;

--
-- Name: warnings_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.warnings_id_seq OWNED BY public.warnings.id;


--
-- Name: yaml_files; Type: TABLE; Schema: public; Owner: sekailink_user
--

CREATE TABLE public.yaml_files (
    id integer NOT NULL,
    user_id character varying,
    filename character varying(100),
    content text,
    game character varying(50),
    updated_at timestamp without time zone
);


ALTER TABLE public.yaml_files OWNER TO sekailink_user;

--
-- Name: yaml_files_id_seq; Type: SEQUENCE; Schema: public; Owner: sekailink_user
--

CREATE SEQUENCE public.yaml_files_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.yaml_files_id_seq OWNER TO sekailink_user;

--
-- Name: yaml_files_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: sekailink_user
--

ALTER SEQUENCE public.yaml_files_id_seq OWNED BY public.yaml_files.id;


--
-- Name: bans id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.bans ALTER COLUMN id SET DEFAULT nextval('public.bans_id_seq'::regclass);


--
-- Name: chat_messages id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.chat_messages ALTER COLUMN id SET DEFAULT nextval('public.chat_messages_id_seq'::regclass);


--
-- Name: custom_worlds id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.custom_worlds ALTER COLUMN id SET DEFAULT nextval('public.custom_worlds_id_seq'::regclass);


--
-- Name: favorite_games id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.favorite_games ALTER COLUMN id SET DEFAULT nextval('public.favorite_games_id_seq'::regclass);


--
-- Name: friends id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.friends ALTER COLUMN id SET DEFAULT nextval('public.friends_id_seq'::regclass);


--
-- Name: games id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.games ALTER COLUMN id SET DEFAULT nextval('public.games_id_seq'::regclass);


--
-- Name: lobbies id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobbies ALTER COLUMN id SET DEFAULT nextval('public.lobbies_id_seq'::regclass);


--
-- Name: lobby_players id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_players ALTER COLUMN id SET DEFAULT nextval('public.lobby_players_id_seq'::regclass);


--
-- Name: lobby_settings id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_settings ALTER COLUMN id SET DEFAULT nextval('public.lobby_settings_id_seq'::regclass);


--
-- Name: rom_files id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.rom_files ALTER COLUMN id SET DEFAULT nextval('public.rom_files_id_seq'::regclass);


--
-- Name: server_ratings id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.server_ratings ALTER COLUMN id SET DEFAULT nextval('public.server_ratings_id_seq'::regclass);


--
-- Name: twitch_connections id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.twitch_connections ALTER COLUMN id SET DEFAULT nextval('public.twitch_connections_id_seq'::regclass);


--
-- Name: user_ratings id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_ratings ALTER COLUMN id SET DEFAULT nextval('public.user_ratings_id_seq'::regclass);


--
-- Name: user_reviews id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_reviews ALTER COLUMN id SET DEFAULT nextval('public.user_reviews_id_seq'::regclass);


--
-- Name: warnings id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.warnings ALTER COLUMN id SET DEFAULT nextval('public.warnings_id_seq'::regclass);


--
-- Name: yaml_files id; Type: DEFAULT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.yaml_files ALTER COLUMN id SET DEFAULT nextval('public.yaml_files_id_seq'::regclass);


--
-- Data for Name: bans; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.bans (id, user_id, banned_by, reason, duration_hours, appeal_text, appeal_status, appeal_reviewed_by, appeal_reviewed_at, created_at, expires_at, is_active) FROM stdin;
\.


--
-- Data for Name: chat_messages; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.chat_messages (id, lobby_id, user_id, message, message_type, is_pinned, pinned_by, deleted, deleted_by, deleted_at, created_at) FROM stdin;
3	11	c698cf4e-0fa2-498e-b947-3a9f25652dca	ihkjhjk	user	f	\N	f	\N	\N	2026-01-04 13:46:24.813686
4	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	test	user	f	\N	f	\N	\N	2026-01-04 16:35:37.401949
5	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	multiworld.gg:60393	user	f	\N	f	\N	\N	2026-01-04 16:35:56.787245
6	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	uiuoi	user	f	\N	f	\N	\N	2026-01-04 16:35:58.76202
7	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	lkjl	user	f	\N	f	\N	\N	2026-01-04 16:36:00.000751
8	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	ujiuoiuo	user	f	\N	f	\N	\N	2026-01-04 16:36:01.134159
9	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	iuoiu	user	f	\N	f	\N	\N	2026-01-04 16:36:02.22225
10	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	ouoiuo	user	f	\N	f	\N	\N	2026-01-04 16:36:03.489994
11	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	iououou	user	f	\N	f	\N	\N	2026-01-04 16:36:04.841307
12	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	<guyiuyiy	user	f	\N	f	\N	\N	2026-01-04 16:36:06.189767
13	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	kiuyiuy	user	f	\N	f	\N	\N	2026-01-04 16:36:07.452054
\.


--
-- Data for Name: custom_worlds; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.custom_worlds (id, name, slug, version, file_path, uploader_id, status, reviewed_by, reviewed_at, rejection_reason, created_at) FROM stdin;
\.


--
-- Data for Name: favorite_games; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.favorite_games (id, user_id, game_slug, created_at) FROM stdin;
\.


--
-- Data for Name: friends; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.friends (id, user_id, friend_id, status, created_at) FROM stdin;
\.


--
-- Data for Name: games; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.games (id, slug, name, description, boxart_url, requires_rom, world_type, sync_count, is_active, created_at) FROM stdin;
1	adventure	Adventure	Archipelago randomizer support for Adventure	\N	f	official	0	t	2026-01-03 15:37:04.591489
2	a-hat-in-time	A Hat in Time	Archipelago randomizer support for A Hat in Time	\N	f	official	0	t	2026-01-03 15:37:04.59463
3	a-link-to-the-past	A Link to the Past	Archipelago randomizer support for A Link to the Past	\N	t	official	0	t	2026-01-03 15:37:04.595855
4	apquest	Apquest	Archipelago randomizer support for Apquest	\N	f	official	0	t	2026-01-03 15:37:04.597023
5	sudoku	Sudoku	Archipelago randomizer support for Sudoku	\N	f	official	0	t	2026-01-03 15:37:04.598136
6	aquaria	Aquaria	Archipelago randomizer support for Aquaria	\N	f	official	0	t	2026-01-03 15:37:04.599514
7	blasphemous	Blasphemous	Archipelago randomizer support for Blasphemous	\N	f	official	0	t	2026-01-03 15:37:04.600796
8	bomb-rush-cyberfunk	Bomb Rush Cyberfunk	Archipelago randomizer support for Bomb Rush Cyberfunk	\N	f	official	0	t	2026-01-03 15:37:04.601799
9	bumper-stickers	Bumper Stickers	Archipelago randomizer support for Bumper Stickers	\N	f	official	0	t	2026-01-03 15:37:04.602939
10	choo-choo-charles	Choo-Choo Charles	Archipelago randomizer support for Choo-Choo Charles	\N	f	official	0	t	2026-01-03 15:37:04.604024
11	celeste-64	Celeste 64	Archipelago randomizer support for Celeste 64	\N	f	official	0	t	2026-01-03 15:37:04.605024
12	celeste-open-world	Celeste (Open World)	Archipelago randomizer support for Celeste (Open World)	\N	f	official	0	t	2026-01-03 15:37:04.606073
13	checksfinder	ChecksFinder	Archipelago randomizer support for ChecksFinder	\N	t	official	0	t	2026-01-03 15:37:04.60704
14	civilization-vi	Civilization VI	Archipelago randomizer support for Civilization VI	\N	f	official	0	t	2026-01-03 15:37:04.608039
15	castlevania-64	Castlevania 64	Archipelago randomizer support for Castlevania 64	\N	f	official	0	t	2026-01-03 15:37:04.60896
16	castlevania-circle-of-the-moon	Castlevania - Circle of the Moon	Archipelago randomizer support for Castlevania - Circle of the Moon	\N	f	official	0	t	2026-01-03 15:37:04.609942
17	dark-souls-iii	Dark Souls III	Archipelago randomizer support for Dark Souls III	\N	t	official	0	t	2026-01-03 15:37:04.610941
18	dkc3	Dkc3	Archipelago randomizer support for Dkc3	\N	f	official	0	t	2026-01-03 15:37:04.611949
19	dlcquest	DLCQuest	Archipelago randomizer support for DLCQuest	\N	f	official	0	t	2026-01-03 15:37:04.612896
20	doom-1993	DOOM 1993	Archipelago randomizer support for DOOM 1993	\N	f	official	0	t	2026-01-03 15:37:04.613861
21	doom-ii	DOOM II	Archipelago randomizer support for DOOM II	\N	f	official	0	t	2026-01-03 15:37:04.614679
22	earthbound	EarthBound	Archipelago randomizer support for EarthBound	\N	f	official	0	t	2026-01-03 15:37:04.615506
23	factorio	Factorio	Archipelago randomizer support for Factorio	\N	f	official	0	t	2026-01-03 15:37:04.61629
24	faxanadu	Faxanadu	Archipelago randomizer support for Faxanadu	\N	f	official	0	t	2026-01-03 15:37:04.617222
25	final-fantasy	Final Fantasy	Archipelago randomizer support for Final Fantasy	\N	t	official	0	t	2026-01-03 15:37:04.618168
26	final-fantasy-mystic-quest	Final Fantasy Mystic Quest	Archipelago randomizer support for Final Fantasy Mystic Quest	\N	f	official	0	t	2026-01-03 15:37:04.619129
27	archipelago	Archipelago	Archipelago randomizer support for Archipelago	\N	f	official	0	t	2026-01-03 15:37:04.620147
28	heretic	Heretic	Archipelago randomizer support for Heretic	\N	f	official	0	t	2026-01-03 15:37:04.620991
29	hollow-knight	Hollow Knight	Archipelago randomizer support for Hollow Knight	\N	t	official	0	t	2026-01-03 15:37:04.621851
30	hylics2	Hylics2	Archipelago randomizer support for Hylics2	\N	f	official	0	t	2026-01-03 15:37:04.622634
31	inscryption	Inscryption	Archipelago randomizer support for Inscryption	\N	f	official	0	t	2026-01-03 15:37:04.62353
32	jakanddaxter	Jakanddaxter	Archipelago randomizer support for Jakanddaxter	\N	f	official	0	t	2026-01-03 15:37:04.624406
33	kirby	Kirby	Archipelago randomizer support for Kirby	\N	f	official	0	t	2026-01-03 15:37:04.625279
34	kingdom-hearts	Kingdom Hearts	Archipelago randomizer support for Kingdom Hearts	\N	f	official	0	t	2026-01-03 15:37:04.626212
35	kingdom-hearts-2	Kingdom Hearts 2	Archipelago randomizer support for Kingdom Hearts 2	\N	f	official	0	t	2026-01-03 15:37:04.627175
36	ladx	Ladx	Archipelago randomizer support for Ladx	\N	f	official	0	t	2026-01-03 15:37:04.627956
37	landstalker-the-treasures-of-king-nole	Landstalker - The Treasures of King Nole	Archipelago randomizer support for Landstalker - The Treasures of King Nole	\N	f	official	0	t	2026-01-03 15:37:04.628856
38	lingo	Lingo	Archipelago randomizer support for Lingo	\N	f	official	0	t	2026-01-03 15:37:04.629816
39	lufia2ac	Lufia2Ac	Archipelago randomizer support for Lufia2Ac	\N	f	official	0	t	2026-01-03 15:37:04.630675
40	super-mario-land-2	Super Mario Land 2	Archipelago randomizer support for Super Mario Land 2	\N	f	official	0	t	2026-01-03 15:37:04.631646
41	meritous	Meritous	Archipelago randomizer support for Meritous	\N	f	official	0	t	2026-01-03 15:37:04.632567
42	the-messenger	The Messenger	Archipelago randomizer support for The Messenger	\N	f	official	0	t	2026-01-03 15:37:04.633559
43	mario-luigi-superstar-saga	Mario & Luigi Superstar Saga	Archipelago randomizer support for Mario & Luigi Superstar Saga	\N	f	official	0	t	2026-01-03 15:37:04.634473
44	mega-man-2	Mega Man 2	Archipelago randomizer support for Mega Man 2	\N	f	official	0	t	2026-01-03 15:37:04.635373
45	megaman-battle-network-3	MegaMan Battle Network 3	Archipelago randomizer support for MegaMan Battle Network 3	\N	f	official	0	t	2026-01-03 15:37:04.636216
46	muse-dash	Muse Dash	Archipelago randomizer support for Muse Dash	\N	f	official	0	t	2026-01-03 15:37:04.637043
47	noita	Noita	Archipelago randomizer support for Noita	\N	f	official	0	t	2026-01-03 15:37:04.637857
48	oot	Oot	Archipelago randomizer support for Oot	\N	f	official	0	t	2026-01-03 15:37:04.638674
49	old-school-runescape	Old School Runescape	Archipelago randomizer support for Old School Runescape	\N	f	official	0	t	2026-01-03 15:37:04.639516
50	overcooked-2	Overcooked! 2	Archipelago randomizer support for Overcooked! 2	\N	t	official	0	t	2026-01-03 15:37:04.640267
51	paint	Paint	Archipelago randomizer support for Paint	\N	f	official	0	t	2026-01-03 15:37:04.641093
52	pokemon-emerald	Pokemon Emerald	Archipelago randomizer support for Pokemon Emerald	\N	t	official	0	t	2026-01-03 15:37:04.642032
53	pokemon-red-and-blue	Pokemon Red and Blue	Archipelago randomizer support for Pokemon Red and Blue	\N	t	official	0	t	2026-01-03 15:37:04.642954
54	raft	Raft	Archipelago randomizer support for Raft	\N	f	official	0	t	2026-01-03 15:37:04.643843
55	risk-of-rain-2	Risk of Rain 2	Archipelago randomizer support for Risk of Rain 2	\N	f	official	0	t	2026-01-03 15:37:04.644643
56	sa2b	Sa2B	Archipelago randomizer support for Sa2B	\N	f	official	0	t	2026-01-03 15:37:04.645494
57	satisfactory	Satisfactory	Archipelago randomizer support for Satisfactory	\N	f	official	0	t	2026-01-03 15:37:04.646345
58	saving-princess	Saving Princess	Archipelago randomizer support for Saving Princess	\N	f	official	0	t	2026-01-03 15:37:04.647375
59	starcraft-2	Starcraft 2	Archipelago randomizer support for Starcraft 2	\N	f	official	0	t	2026-01-03 15:37:04.648336
60	shapez	Shapez	Archipelago randomizer support for Shapez	\N	f	official	0	t	2026-01-03 15:37:04.649277
61	shivers	Shivers	Archipelago randomizer support for Shivers	\N	f	official	0	t	2026-01-03 15:37:04.65041
62	a-short-hike	A Short Hike	Archipelago randomizer support for A Short Hike	\N	f	official	0	t	2026-01-03 15:37:04.651429
63	super-metroid	Super Metroid	Archipelago randomizer support for Super Metroid	\N	t	official	0	t	2026-01-03 15:37:04.652393
64	sm64ex	Sm64Ex	Archipelago randomizer support for Sm64Ex	\N	f	official	0	t	2026-01-03 15:37:04.653341
65	smw	Smw	Archipelago randomizer support for Smw	\N	f	official	0	t	2026-01-03 15:37:04.654222
66	smz3	SMZ3	Archipelago randomizer support for SMZ3	\N	f	official	0	t	2026-01-03 15:37:04.655154
67	soe	Soe	Archipelago randomizer support for Soe	\N	f	official	0	t	2026-01-03 15:37:04.656062
68	stardew-valley	Stardew Valley	Archipelago randomizer support for Stardew Valley	\N	f	official	0	t	2026-01-03 15:37:04.656915
69	subnautica	Subnautica	Archipelago randomizer support for Subnautica	\N	f	official	0	t	2026-01-03 15:37:04.657736
70	terraria	Terraria	Archipelago randomizer support for Terraria	\N	f	official	0	t	2026-01-03 15:37:04.658631
71	timespinner	Timespinner	Archipelago randomizer support for Timespinner	\N	t	official	0	t	2026-01-03 15:37:04.659503
72	the-legend-of-zelda	The Legend of Zelda	Archipelago randomizer support for The Legend of Zelda	\N	t	official	0	t	2026-01-03 15:37:04.660382
73	tunic	TUNIC	Archipelago randomizer support for TUNIC	\N	f	official	0	t	2026-01-03 15:37:04.661283
74	tww	Tww	Archipelago randomizer support for Tww	\N	f	official	0	t	2026-01-03 15:37:04.662209
75	undertale	Undertale	Archipelago randomizer support for Undertale	\N	f	official	0	t	2026-01-03 15:37:04.663159
76	v6	V6	Archipelago randomizer support for V6	\N	f	official	0	t	2026-01-03 15:37:04.664145
77	wargroove	Wargroove	Archipelago randomizer support for Wargroove	\N	f	official	0	t	2026-01-03 15:37:04.665211
78	the-witness	The Witness	Archipelago randomizer support for The Witness	\N	f	official	0	t	2026-01-03 15:37:04.666126
79	yachtdice	Yachtdice	Archipelago randomizer support for Yachtdice	\N	f	official	0	t	2026-01-03 15:37:04.667137
80	yoshi	Yoshi	Archipelago randomizer support for Yoshi	\N	f	official	0	t	2026-01-03 15:37:04.668099
81	yu-gi-oh-2006	Yu-Gi-Oh! 2006	Archipelago randomizer support for Yu-Gi-Oh! 2006	\N	f	official	0	t	2026-01-03 15:37:04.668967
82	zillion	Zillion	Archipelago randomizer support for Zillion	\N	f	official	0	t	2026-01-03 15:37:04.669734
83	super-mario-world	Super Mario World	Archipelago randomizer support for Super Mario World	\N	t	official	\N	t	\N
\.


--
-- Data for Name: lobbies; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.lobbies (id, host_id, name, slug, status, visibility, seed_url, server_port, time_limit_hours, restrict_time_limit, timer_started_at, created_at, started_at, ended_at) FROM stdin;
5	c698cf4e-0fa2-498e-b947-3a9f25652dca	Test	awesome-champion-7478	open	open	\N	\N	\N	f	\N	2026-01-04 06:35:09.803073	\N	\N
6	c698cf4e-0fa2-498e-b947-3a9f25652dca	test	crazy-warrior-1771	open	open	\N	\N	\N	f	\N	2026-01-04 06:59:07.616038	\N	\N
7	c698cf4e-0fa2-498e-b947-3a9f25652dca	test generate	lucky-master-4157	open	open	\N	\N	\N	f	\N	2026-01-04 06:59:31.46198	\N	\N
8	c698cf4e-0fa2-498e-b947-3a9f25652dca	test v2	awesome-legend-5682	open	open	\N	\N	\N	f	\N	2026-01-04 12:48:45.09907	\N	\N
9	c698cf4e-0fa2-498e-b947-3a9f25652dca	test v2-6	happy-pro-6809	open	open	\N	\N	\N	f	\N	2026-01-04 12:49:06.412245	\N	\N
10	c698cf4e-0fa2-498e-b947-3a9f25652dca	test again	wild-pro-5726	open	open	\N	\N	\N	f	\N	2026-01-04 13:36:08.205697	\N	\N
11	c698cf4e-0fa2-498e-b947-3a9f25652dca	prout test	brave-gamer-5279	open	open	\N	\N	\N	f	\N	2026-01-04 13:46:15.607585	\N	\N
12	c698cf4e-0fa2-498e-b947-3a9f25652dca	Test	wild-runner-1899	open	open	\N	\N	\N	f	\N	2026-01-04 14:55:23.157668	\N	\N
\.


--
-- Data for Name: lobby_players; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.lobby_players (id, lobby_id, user_id, game, yaml_file_id, rom_file_id, is_ready, status, patch_url, joined_at, finished_at) FROM stdin;
5	5	c698cf4e-0fa2-498e-b947-3a9f25652dca	game	2	\N	t	waiting	\N	2026-01-04 06:35:09.806917	\N
6	6	c698cf4e-0fa2-498e-b947-3a9f25652dca		\N	\N	f	waiting	\N	2026-01-04 06:59:07.619986	\N
7	7	c698cf4e-0fa2-498e-b947-3a9f25652dca		\N	\N	f	waiting	\N	2026-01-04 06:59:31.463288	\N
8	8	c698cf4e-0fa2-498e-b947-3a9f25652dca		\N	\N	f	waiting	\N	2026-01-04 12:48:45.105192	\N
9	9	c698cf4e-0fa2-498e-b947-3a9f25652dca	game	2	\N	t	waiting	\N	2026-01-04 12:49:06.413898	\N
10	10	c698cf4e-0fa2-498e-b947-3a9f25652dca	game	2	\N	t	waiting	\N	2026-01-04 13:36:08.208495	\N
11	11	c698cf4e-0fa2-498e-b947-3a9f25652dca	game	2	\N	t	waiting	\N	2026-01-04 13:46:15.610368	\N
12	12	c698cf4e-0fa2-498e-b947-3a9f25652dca	Pokemon Emerald	2	\N	t	waiting	\N	2026-01-04 14:55:23.16044	\N
\.


--
-- Data for Name: lobby_settings; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.lobby_settings (id, lobby_id, max_players, time_limit_hours, sync_rules, allow_multigame, allow_broadcast, disallow_rom_games, disallow_custom_worlds, voice_chat_enabled, blacklisted_games) FROM stdin;
5	5	10	\N		t	t	f	f	t	[]
6	6	10	\N		t	t	f	f	t	[]
7	7	10	\N		t	t	f	f	t	[]
8	8	10	\N		t	t	f	f	t	[]
9	9	10	\N		t	t	f	f	t	[]
10	10	10	\N		t	t	f	f	t	[]
11	11	10	6		t	t	f	f	t	[]
12	12	10	\N		t	t	f	f	t	[]
\.


--
-- Data for Name: rom_files; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.rom_files (id, user_id, filename, file_path, sha1, game_detected, status, uploaded_at, lobby_id) FROM stdin;
1	c698cf4e-0fa2-498e-b947-3a9f25652dca	Pokemon_-_Emerald_Version_USA_Europe.gba	/tmp/lobbies/12/c698cf4e-0fa2-498e-b947-3a9f25652dca_Pokemon_-_Emerald_Version_USA_Europe.gba	f3ae088181bf583e55daf962a92bb46f4f1d07b7	Pokemon Emerald	uploaded	2026-01-04 14:56:17.173052	12
\.


--
-- Data for Name: server_ratings; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.server_ratings (id, user_id, kicks_count, bans_count, suspensions_count, syncs_finished, syncs_ragequit, rating, updated_at) FROM stdin;
\.


--
-- Data for Name: twitch_connections; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.twitch_connections (id, user_id, twitch_id, twitch_username, access_token, refresh_token, token_expires_at, created_at, updated_at) FROM stdin;
\.


--
-- Data for Name: user_ratings; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.user_ratings (id, from_user_id, to_user_id, punctuality, respect_others, respect_rules, valid_release, overall_rating, review_text, created_at) FROM stdin;
\.


--
-- Data for Name: user_reviews; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.user_reviews (id, from_user_id, to_user_id, review_text, likes, dislikes, reports, status, moderated_by, moderated_at, created_at) FROM stdin;
\.


--
-- Data for Name: users; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.users (id, discord_id, username, avatar_url, email, email_verified, pronouns, bio, language, role, is_banned, is_suspended, last_seen, badge_supporter, badge_host_master, badge_sync_gamer, created_at) FROM stdin;
3e7272c6-0ea8-43aa-8c2e-7349630e6559	142043310045790209	themalaki	https://cdn.discordapp.com/avatars/142043310045790209/2ca7af1fd46fdf580ee8c143f103ab79.png	michaud493@gmail.com	f	\N	\N	en	admin	f	f	2026-01-03 16:25:50.876149	f	f	f	2026-01-03 16:25:50.876153
c698cf4e-0fa2-498e-b947-3a9f25652dca	83238976651001856	thelovenityjade	https://cdn.discordapp.com/avatars/83238976651001856/68088cf5f41093bdc7aa29c6010e7d27.png	jflauzon.pab@gmail.com	f	she/her 	I'm an admin.	en	admin	f	f	2026-01-04 19:40:36.91942	f	f	f	2026-01-03 15:43:56.573092
\.


--
-- Data for Name: warnings; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.warnings (id, user_id, warned_by, reason, created_at) FROM stdin;
\.


--
-- Data for Name: yaml_files; Type: TABLE DATA; Schema: public; Owner: sekailink_user
--

COPY public.yaml_files (id, user_id, filename, content, game, updated_at) FROM stdin;
1	3e7272c6-0ea8-43aa-8c2e-7349630e6559	untitled.yaml	# New YAML Configuration\nname: Malaki_SMW\ngame: Super Mario World\ndescription: Generated by https://archipelago.gg/ for Super Mario World\nSuper Mario World:\n  progression_balancing: '50'\n  accessibility: full\n  death_link: 'false'\n  ring_link: 'false'\n  trap_link: 'false'\n  early_climb: 'false'\n  goal: bowser\n  bosses_required: '7'\n  max_yoshi_egg_cap: '50'\n  percentage_of_yoshi_eggs: '100'\n  dragon_coin_checks: 'false'\n  moon_checks: 'false'\n  hidden_1up_checks: 'false'\n  bonus_block_checks: 'false'\n  blocksanity: 'false'\n  level_shuffle: 'true'\n  exclude_special_zone: 'false'\n  bowser_castle_doors: vanilla\n  bowser_castle_rooms: random_two_room\n  boss_shuffle: full\n  swap_donut_gh_exits: 'false'\n  junk_fill_percentage: '0'\n  trap_fill_percentage: '0'\n  ice_trap_weight: medium\n  stun_trap_weight: medium\n  literature_trap_weight: medium\n  timer_trap_weight: medium\n  reverse_trap_weight: medium\n  thwimp_trap_weight: medium\n  display_received_item_popups: progression_minus_yoshi_eggs\n  autosave: 'true'\n  overworld_speed: fast\n  music_shuffle: full\n  sfx_shuffle: none\n  mario_palette: mario\n  level_palette_shuffle: 'off'\n  overworld_palette_shuffle: 'off'\n  starting_life_count: '99'\n  local_items: []\n  non_local_items: []\n  start_hints: []\n  start_location_hints: []\n  exclude_locations: []\n  priority_locations: []\n  start_inventory: {}\n	Super Mario World	2026-01-03 20:43:37.266663
2	c698cf4e-0fa2-498e-b947-3a9f25652dca	PokemonEmerald.yaml	name: TheLovenityJade\r\ngame: Pokemon Emerald\r\ndescription: Generated by https://archipelago.gg/ for Pokemon Emerald\r\nPokemon Emerald:\r\n  progression_balancing: '50'\r\n  accessibility: full\r\n  goal: champion\r\n  badges: completely_random\r\n  hms: completely_random\r\n  key_items: 'true'\r\n  bikes: 'true'\r\n  event_tickets: 'false'\r\n  rods: 'true'\r\n  overworld_items: 'true'\r\n  hidden_items: 'true'\r\n  npc_gifts: 'true'\r\n  berry_trees: 'true'\r\n  dexsanity: 'false'\r\n  trainersanity: 'false'\r\n  item_pool_type: diverse_balanced\r\n  require_itemfinder: 'false'\r\n  require_flash: neither\r\n  elite_four_requirement: badges\r\n  elite_four_count: '8'\r\n  norman_requirement: badges\r\n  norman_count: '4'\r\n  legendary_hunt_catch: 'false'\r\n  legendary_hunt_count: '3'\r\n  allowed_legendary_hunt_encounters:\r\n  - Groudon\r\n  - Kyogre\r\n  - Rayquaza\r\n  - Latios\r\n  - Latias\r\n  - Regirock\r\n  - Registeel\r\n  - Regice\r\n  - Ho-Oh\r\n  - Lugia\r\n  - Deoxys\r\n  - Mew\r\n  wild_pokemon: match_base_stats\r\n  wild_encounter_blacklist: []\r\n  starters: match_base_stats\r\n  starter_blacklist: []\r\n  trainer_parties: vanilla\r\n  trainer_party_blacklist: []\r\n  force_fully_evolved: '100'\r\n  legendary_encounters: match_base_stats\r\n  misc_pokemon: match_base_stats\r\n  types: vanilla\r\n  abilities: vanilla\r\n  ability_blacklist: []\r\n  level_up_moves: vanilla\r\n  move_match_type_bias: '0'\r\n  move_normal_type_bias: '0'\r\n  tm_tutor_compatibility: '-1'\r\n  hm_compatibility: '-1'\r\n  tm_tutor_moves: 'false'\r\n  reusable_tms_tutors: 'false'\r\n  move_blacklist: []\r\n  min_catch_rate: '3'\r\n  guaranteed_catch: 'true'\r\n  normalize_encounter_rates: 'false'\r\n  exp_modifier: '283'\r\n  blind_trainers: 'true'\r\n  purge_spinners: 'false'\r\n  match_trainer_levels: 'off'\r\n  match_trainer_levels_bonus: '0'\r\n  double_battle_chance: '0'\r\n  better_shops: 'true'\r\n  remove_roadblocks: []\r\n  extra_boulders: 'false'\r\n  extra_bumpy_slope: 'false'\r\n  modify_118: 'false'\r\n  free_fly_location: 'false'\r\n  free_fly_blacklist:\r\n  - Littleroot Town\r\n  - Oldale Town\r\n  - Petalburg City\r\n  - Rustboro City\r\n  - Dewford Town\r\n  hm_requirements: fly_without_badge\r\n  turbo_a: 'true'\r\n  receive_item_messages: progression\r\n  remote_items: 'false'\r\n  music: 'false'\r\n  fanfares: 'false'\r\n  death_link: 'false'\r\n  enable_wonder_trading: 'true'\r\n  easter_egg: EMERALD SECRET\r\n  local_items: []\r\n  non_local_items: []\r\n  start_hints: []\r\n  start_location_hints: []\r\n  exclude_locations: []\r\n  priority_locations: []\r\n  start_inventory: {}\r\n	Pokemon Emerald	2026-01-04 01:26:10.239918
\.


--
-- Name: bans_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.bans_id_seq', 1, false);


--
-- Name: chat_messages_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.chat_messages_id_seq', 13, true);


--
-- Name: custom_worlds_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.custom_worlds_id_seq', 1, false);


--
-- Name: favorite_games_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.favorite_games_id_seq', 1, false);


--
-- Name: friends_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.friends_id_seq', 1, false);


--
-- Name: games_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.games_id_seq', 83, true);


--
-- Name: lobbies_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.lobbies_id_seq', 12, true);


--
-- Name: lobby_players_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.lobby_players_id_seq', 12, true);


--
-- Name: lobby_settings_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.lobby_settings_id_seq', 12, true);


--
-- Name: rom_files_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.rom_files_id_seq', 1, true);


--
-- Name: server_ratings_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.server_ratings_id_seq', 1, false);


--
-- Name: twitch_connections_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.twitch_connections_id_seq', 1, false);


--
-- Name: user_ratings_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.user_ratings_id_seq', 1, false);


--
-- Name: user_reviews_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.user_reviews_id_seq', 1, false);


--
-- Name: warnings_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.warnings_id_seq', 1, false);


--
-- Name: yaml_files_id_seq; Type: SEQUENCE SET; Schema: public; Owner: sekailink_user
--

SELECT pg_catalog.setval('public.yaml_files_id_seq', 2, true);


--
-- Name: friends _user_friend_uc; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.friends
    ADD CONSTRAINT _user_friend_uc UNIQUE (user_id, friend_id);


--
-- Name: favorite_games _user_game_uc; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.favorite_games
    ADD CONSTRAINT _user_game_uc UNIQUE (user_id, game_slug);


--
-- Name: user_ratings _user_rating_uc; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_ratings
    ADD CONSTRAINT _user_rating_uc UNIQUE (from_user_id, to_user_id);


--
-- Name: bans bans_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.bans
    ADD CONSTRAINT bans_pkey PRIMARY KEY (id);


--
-- Name: chat_messages chat_messages_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.chat_messages
    ADD CONSTRAINT chat_messages_pkey PRIMARY KEY (id);


--
-- Name: custom_worlds custom_worlds_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.custom_worlds
    ADD CONSTRAINT custom_worlds_pkey PRIMARY KEY (id);


--
-- Name: custom_worlds custom_worlds_slug_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.custom_worlds
    ADD CONSTRAINT custom_worlds_slug_key UNIQUE (slug);


--
-- Name: favorite_games favorite_games_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.favorite_games
    ADD CONSTRAINT favorite_games_pkey PRIMARY KEY (id);


--
-- Name: friends friends_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.friends
    ADD CONSTRAINT friends_pkey PRIMARY KEY (id);


--
-- Name: games games_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.games
    ADD CONSTRAINT games_pkey PRIMARY KEY (id);


--
-- Name: games games_slug_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.games
    ADD CONSTRAINT games_slug_key UNIQUE (slug);


--
-- Name: lobbies lobbies_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobbies
    ADD CONSTRAINT lobbies_pkey PRIMARY KEY (id);


--
-- Name: lobbies lobbies_slug_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobbies
    ADD CONSTRAINT lobbies_slug_key UNIQUE (slug);


--
-- Name: lobby_players lobby_players_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_players
    ADD CONSTRAINT lobby_players_pkey PRIMARY KEY (id);


--
-- Name: lobby_settings lobby_settings_lobby_id_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_settings
    ADD CONSTRAINT lobby_settings_lobby_id_key UNIQUE (lobby_id);


--
-- Name: lobby_settings lobby_settings_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_settings
    ADD CONSTRAINT lobby_settings_pkey PRIMARY KEY (id);


--
-- Name: rom_files rom_files_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.rom_files
    ADD CONSTRAINT rom_files_pkey PRIMARY KEY (id);


--
-- Name: server_ratings server_ratings_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.server_ratings
    ADD CONSTRAINT server_ratings_pkey PRIMARY KEY (id);


--
-- Name: server_ratings server_ratings_user_id_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.server_ratings
    ADD CONSTRAINT server_ratings_user_id_key UNIQUE (user_id);


--
-- Name: twitch_connections twitch_connections_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.twitch_connections
    ADD CONSTRAINT twitch_connections_pkey PRIMARY KEY (id);


--
-- Name: twitch_connections twitch_connections_user_id_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.twitch_connections
    ADD CONSTRAINT twitch_connections_user_id_key UNIQUE (user_id);


--
-- Name: user_ratings user_ratings_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_ratings
    ADD CONSTRAINT user_ratings_pkey PRIMARY KEY (id);


--
-- Name: user_reviews user_reviews_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_reviews
    ADD CONSTRAINT user_reviews_pkey PRIMARY KEY (id);


--
-- Name: users users_discord_id_key; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.users
    ADD CONSTRAINT users_discord_id_key UNIQUE (discord_id);


--
-- Name: users users_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.users
    ADD CONSTRAINT users_pkey PRIMARY KEY (id);


--
-- Name: warnings warnings_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.warnings
    ADD CONSTRAINT warnings_pkey PRIMARY KEY (id);


--
-- Name: yaml_files yaml_files_pkey; Type: CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.yaml_files
    ADD CONSTRAINT yaml_files_pkey PRIMARY KEY (id);


--
-- Name: bans bans_appeal_reviewed_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.bans
    ADD CONSTRAINT bans_appeal_reviewed_by_fkey FOREIGN KEY (appeal_reviewed_by) REFERENCES public.users(id);


--
-- Name: bans bans_banned_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.bans
    ADD CONSTRAINT bans_banned_by_fkey FOREIGN KEY (banned_by) REFERENCES public.users(id);


--
-- Name: bans bans_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.bans
    ADD CONSTRAINT bans_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: chat_messages chat_messages_deleted_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.chat_messages
    ADD CONSTRAINT chat_messages_deleted_by_fkey FOREIGN KEY (deleted_by) REFERENCES public.users(id);


--
-- Name: chat_messages chat_messages_lobby_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.chat_messages
    ADD CONSTRAINT chat_messages_lobby_id_fkey FOREIGN KEY (lobby_id) REFERENCES public.lobbies(id);


--
-- Name: chat_messages chat_messages_pinned_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.chat_messages
    ADD CONSTRAINT chat_messages_pinned_by_fkey FOREIGN KEY (pinned_by) REFERENCES public.users(id);


--
-- Name: chat_messages chat_messages_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.chat_messages
    ADD CONSTRAINT chat_messages_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: custom_worlds custom_worlds_reviewed_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.custom_worlds
    ADD CONSTRAINT custom_worlds_reviewed_by_fkey FOREIGN KEY (reviewed_by) REFERENCES public.users(id);


--
-- Name: custom_worlds custom_worlds_uploader_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.custom_worlds
    ADD CONSTRAINT custom_worlds_uploader_id_fkey FOREIGN KEY (uploader_id) REFERENCES public.users(id);


--
-- Name: favorite_games favorite_games_game_slug_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.favorite_games
    ADD CONSTRAINT favorite_games_game_slug_fkey FOREIGN KEY (game_slug) REFERENCES public.games(slug);


--
-- Name: favorite_games favorite_games_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.favorite_games
    ADD CONSTRAINT favorite_games_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: friends friends_friend_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.friends
    ADD CONSTRAINT friends_friend_id_fkey FOREIGN KEY (friend_id) REFERENCES public.users(id);


--
-- Name: friends friends_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.friends
    ADD CONSTRAINT friends_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: lobbies lobbies_host_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobbies
    ADD CONSTRAINT lobbies_host_id_fkey FOREIGN KEY (host_id) REFERENCES public.users(id);


--
-- Name: lobby_players lobby_players_lobby_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_players
    ADD CONSTRAINT lobby_players_lobby_id_fkey FOREIGN KEY (lobby_id) REFERENCES public.lobbies(id);


--
-- Name: lobby_players lobby_players_rom_file_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_players
    ADD CONSTRAINT lobby_players_rom_file_id_fkey FOREIGN KEY (rom_file_id) REFERENCES public.rom_files(id);


--
-- Name: lobby_players lobby_players_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_players
    ADD CONSTRAINT lobby_players_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: lobby_players lobby_players_yaml_file_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_players
    ADD CONSTRAINT lobby_players_yaml_file_id_fkey FOREIGN KEY (yaml_file_id) REFERENCES public.yaml_files(id);


--
-- Name: lobby_settings lobby_settings_lobby_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.lobby_settings
    ADD CONSTRAINT lobby_settings_lobby_id_fkey FOREIGN KEY (lobby_id) REFERENCES public.lobbies(id);


--
-- Name: rom_files rom_files_lobby_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.rom_files
    ADD CONSTRAINT rom_files_lobby_id_fkey FOREIGN KEY (lobby_id) REFERENCES public.lobbies(id) ON DELETE CASCADE;


--
-- Name: rom_files rom_files_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.rom_files
    ADD CONSTRAINT rom_files_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: server_ratings server_ratings_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.server_ratings
    ADD CONSTRAINT server_ratings_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: twitch_connections twitch_connections_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.twitch_connections
    ADD CONSTRAINT twitch_connections_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: user_ratings user_ratings_from_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_ratings
    ADD CONSTRAINT user_ratings_from_user_id_fkey FOREIGN KEY (from_user_id) REFERENCES public.users(id);


--
-- Name: user_ratings user_ratings_to_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_ratings
    ADD CONSTRAINT user_ratings_to_user_id_fkey FOREIGN KEY (to_user_id) REFERENCES public.users(id);


--
-- Name: user_reviews user_reviews_from_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_reviews
    ADD CONSTRAINT user_reviews_from_user_id_fkey FOREIGN KEY (from_user_id) REFERENCES public.users(id);


--
-- Name: user_reviews user_reviews_moderated_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_reviews
    ADD CONSTRAINT user_reviews_moderated_by_fkey FOREIGN KEY (moderated_by) REFERENCES public.users(id);


--
-- Name: user_reviews user_reviews_to_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.user_reviews
    ADD CONSTRAINT user_reviews_to_user_id_fkey FOREIGN KEY (to_user_id) REFERENCES public.users(id);


--
-- Name: warnings warnings_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.warnings
    ADD CONSTRAINT warnings_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- Name: warnings warnings_warned_by_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.warnings
    ADD CONSTRAINT warnings_warned_by_fkey FOREIGN KEY (warned_by) REFERENCES public.users(id);


--
-- Name: yaml_files yaml_files_user_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: sekailink_user
--

ALTER TABLE ONLY public.yaml_files
    ADD CONSTRAINT yaml_files_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id);


--
-- PostgreSQL database dump complete
--

\unrestrict 4dqMmFOlYAXApa2NaVwfVztuaofoCpixlMOV9ZPZFH1dWa5BhSpSCNwgxwzWGvy

