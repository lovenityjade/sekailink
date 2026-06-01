START TRANSACTION;

UPDATE games
SET system_key = 'soh',
    display_name = 'Ship of Harkinian',
    active_linkedworld_id = 'apworld.oot_soh',
    status = 'active'
WHERE game_key = 'ship_of_harkinian';

DELETE c FROM game_option_choices c
JOIN game_option_definitions d ON d.id = c.option_id
JOIN game_option_schema_versions s ON s.id = d.schema_version_id
JOIN games g ON g.id = s.game_id
WHERE g.game_key = 'ocarina_of_time';

DELETE d FROM game_option_definitions d
JOIN game_option_schema_versions s ON s.id = d.schema_version_id
JOIN games g ON g.id = s.game_id
WHERE g.game_key = 'ocarina_of_time';

DELETE grp FROM game_option_groups grp
JOIN game_option_schema_versions s ON s.id = grp.schema_version_id
JOIN games g ON g.id = s.game_id
WHERE g.game_key = 'ocarina_of_time';

UPDATE games
SET active_option_schema_id = NULL
WHERE game_key = 'ocarina_of_time';

DELETE s FROM game_option_schema_versions s
JOIN games g ON g.id = s.game_id
WHERE g.game_key = 'ocarina_of_time';

DELETE FROM games
WHERE game_key = 'ocarina_of_time';

COMMIT;
