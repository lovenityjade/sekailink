# Change Control

Ce dossier definit la gouvernance des futures modifications liees a SekaiLink
Core Access.

## Regles

- Toute modification code doit avoir sa documentation dans le meme commit.
- Les changements serveur/client/emu doivent documenter leur impact.
- Les changements de connectivite Client Core, Sekaiemu ou SKLMI exigent un
  contrat complet.
- Toute modification SKLMI exige un accord explicite ecrit de Jade.
- Les changements urgents live peuvent etre patches d'abord, mais doivent etre
  documentes immediatement dans la meme session.

## Templates

- [Change record](template-change-record.md)
- [Connectivity contract](template-connectivity-contract.md)
- [SKLMI approval request](template-sklmi-approval-request.md)
- [Emergency change policy](emergency-change-policy.md)
- [Platform sync policy](platform-sync-policy.md)
- [Changelog](changelog.md)

