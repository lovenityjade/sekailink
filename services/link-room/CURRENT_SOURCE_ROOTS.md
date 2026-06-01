# Current Source Roots

Source monorepo courante:

- `servers/link/server/`
- `servers/link/deploy/`
- `servers/link/docs/`
- `servers/link/scripts/`

Tri impose:

- extraire tout ce qui releve de room live, session, readiness, slots, sync,
  routing gameplay
- ne pas emmener le social live, les contacts ou le chat global

Etat:

- premiere slice room/lobby/admin copiee
- monolithe complet preserve en snapshot pour tri ulterieur
