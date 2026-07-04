START TRANSACTION;

UPDATE games
SET system_key = CASE game_key
  WHEN 'final_fantasy' THEN 'nes'
  WHEN 'mega_man_2' THEN 'nes'
  WHEN 'mega_man_3' THEN 'nes'
  WHEN 'the_legend_of_zelda' THEN 'nes'
  WHEN 'zelda_ii_the_adventure_of_link' THEN 'nes'

  WHEN 'a_link_to_the_past' THEN 'snes'
  WHEN 'chrono_trigger_jets_of_time' THEN 'snes'
  WHEN 'donkey_kong_country' THEN 'snes'
  WHEN 'donkey_kong_country_2' THEN 'snes'
  WHEN 'earthbound' THEN 'snes'
  WHEN 'final_fantasy_iv_free_enterprise' THEN 'snes'
  WHEN 'final_fantasy_mystic_quest' THEN 'snes'
  WHEN 'final_fantasy_v_career_day' THEN 'snes'
  WHEN 'kirby_s_dream_land_3' THEN 'snes'
  WHEN 'lufia_ii_ancient_cave' THEN 'snes'
  WHEN 'mega_man_x3' THEN 'snes'
  WHEN 'secret_of_evermore' THEN 'snes'
  WHEN 'super_mario_world' THEN 'snes'
  WHEN 'super_metroid' THEN 'snes'
  WHEN 'super_metroid_map_rando' THEN 'snes'
  WHEN 'tetris_attack' THEN 'snes'
  WHEN 'yoshi_s_island' THEN 'snes'

  WHEN 'super_mario_land_2' THEN 'gb'
  WHEN 'wario_land' THEN 'gb'
  WHEN 'links_awakening_dx_beta' THEN 'gbc'
  WHEN 'pokemon_crystal' THEN 'gbc'
  WHEN 'pokemon_red_and_blue' THEN 'gbc'
  WHEN 'the_legend_of_zelda_oracle_of_ages' THEN 'gbc'
  WHEN 'the_legend_of_zelda_oracle_of_seasons' THEN 'gbc'

  WHEN 'castlevania_circle_of_the_moon' THEN 'gba'
  WHEN 'final_fantasy_tactics_advance' THEN 'gba'
  WHEN 'golden_sun_the_lost_age' THEN 'gba'
  WHEN 'mario_and_luigi_superstar_saga' THEN 'gba'
  WHEN 'metroid_fusion' THEN 'gba'
  WHEN 'metroid_zero_mission' THEN 'gba'
  WHEN 'pokemon_emerald' THEN 'gba'
  WHEN 'pokemon_firered_and_leafgreen' THEN 'gba'
  WHEN 'the_minish_cap' THEN 'gba'
  WHEN 'wario_land_4' THEN 'gba'
  WHEN 'yu_gi_oh_2006' THEN 'gba'
  WHEN 'yu_gi_oh_dungeon_dice_monsters' THEN 'gba'

  WHEN 'luigi_s_mansion' THEN 'gamecube'
  WHEN 'mario_kart_double_dash' THEN 'gamecube'
  WHEN 'metroid_prime' THEN 'gamecube'
  WHEN 'paper_mario_the_thousand_year_door' THEN 'gamecube'
  WHEN 'sonic_adventure_2_battle' THEN 'gamecube'
  WHEN 'sonic_adventure_dx' THEN 'gamecube'
  WHEN 'sonic_heroes' THEN 'gamecube'
  WHEN 'super_mario_sunshine' THEN 'gamecube'
  WHEN 'the_wind_waker' THEN 'gamecube'
  WHEN 'twilight_princess' THEN 'gamecube'

  WHEN 'a_link_between_worlds' THEN '3ds'
  WHEN 'symphony_of_the_night' THEN 'psx'
  WHEN 'ship_of_harkinian' THEN 'soh'
  ELSE system_key
END
WHERE game_key IN (
  'final_fantasy',
  'mega_man_2',
  'mega_man_3',
  'the_legend_of_zelda',
  'zelda_ii_the_adventure_of_link',
  'a_link_to_the_past',
  'chrono_trigger_jets_of_time',
  'donkey_kong_country',
  'donkey_kong_country_2',
  'earthbound',
  'final_fantasy_iv_free_enterprise',
  'final_fantasy_mystic_quest',
  'final_fantasy_v_career_day',
  'kirby_s_dream_land_3',
  'lufia_ii_ancient_cave',
  'mega_man_x3',
  'secret_of_evermore',
  'super_mario_world',
  'super_metroid',
  'super_metroid_map_rando',
  'tetris_attack',
  'yoshi_s_island',
  'super_mario_land_2',
  'wario_land',
  'links_awakening_dx_beta',
  'pokemon_crystal',
  'pokemon_red_and_blue',
  'the_legend_of_zelda_oracle_of_ages',
  'the_legend_of_zelda_oracle_of_seasons',
  'castlevania_circle_of_the_moon',
  'final_fantasy_tactics_advance',
  'golden_sun_the_lost_age',
  'mario_and_luigi_superstar_saga',
  'metroid_fusion',
  'metroid_zero_mission',
  'pokemon_emerald',
  'pokemon_firered_and_leafgreen',
  'the_minish_cap',
  'wario_land_4',
  'yu_gi_oh_2006',
  'yu_gi_oh_dungeon_dice_monsters',
  'luigi_s_mansion',
  'mario_kart_double_dash',
  'metroid_prime',
  'paper_mario_the_thousand_year_door',
  'sonic_adventure_2_battle',
  'sonic_adventure_dx',
  'sonic_heroes',
  'super_mario_sunshine',
  'the_wind_waker',
  'twilight_princess',
  'a_link_between_worlds',
  'symphony_of_the_night',
  'ship_of_harkinian'
);

COMMIT;
