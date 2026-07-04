<?php

declare(strict_types=1);

return [
    'site_name' => 'SekaiLink',
    'tagline' => 'Multiworld sessions without the setup drag.',
    'base_url' => getenv('SEKAILINK_BASE_URL') ?: 'https://sekailink.com',
    'discord_url' => getenv('SEKAILINK_DISCORD_URL') ?: 'https://discord.gg/jTaefxAEDW',
    'download_url' => getenv('SEKAILINK_DOWNLOAD_URL') ?: '#download',
    'room_browser_url' => getenv('SEKAILINK_ROOM_BROWSER_URL') ?: '/rooms',
    'login_url' => getenv('SEKAILINK_LOGIN_URL') ?: '/login',
    'register_url' => getenv('SEKAILINK_REGISTER_URL') ?: '/register',
    'support_email' => getenv('SEKAILINK_SUPPORT_EMAIL') ?: 'support@sekailink.com',
    'status_label' => getenv('SEKAILINK_STATUS_LABEL') ?: 'Public test online',
    'download_label' => getenv('SEKAILINK_DOWNLOAD_LABEL') ?: 'Windows public test build',
    'hero_title' => 'Start your multiworld in minutes, not hours.',
    'hero_subtitle' => 'SekaiLink unifies room runtime, tracker flow, and server orchestration into one cleaner platform built for players, streamers, and support.',
    'hero_micro' => 'Native server stack in progress. Faster setup, clearer state, better diagnostics.',
    'feature_cards' => [
        [
            'icon' => '⚡',
            'title' => 'Native Room Runtime',
            'body' => 'Room state, audit, projections, and runtime endpoints are moving into a native C++ stack designed around SekaiLink contracts.',
        ],
        [
            'icon' => '🧭',
            'title' => 'Structured State',
            'body' => 'Clients read room state, items, checks, and milestones as structured data instead of scraping logs.',
        ],
        [
            'icon' => '🛡',
            'title' => 'Operational Control',
            'body' => 'Admin agents, verbose room audit, and centralized reports keep incidents debuggable without shell-heavy workflows.',
        ],
        [
            'icon' => '📡',
            'title' => 'Async-Ready Direction',
            'body' => 'Live rooms, async rooms, mobile summaries, and social services are part of the same long-term server plan.',
        ],
    ],
    'games' => [
        'A Link to the Past',
        'Pokemon Emerald',
        'Ocarina of Time',
        'Super Metroid',
        'Super Mario World',
        'SMZ3',
        'The Minish Cap',
        'Ship of Harkinian',
    ],
];
