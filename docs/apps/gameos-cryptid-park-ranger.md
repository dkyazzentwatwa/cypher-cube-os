# Cryptid Ranger

Status: `campaign_lite`

Source route: Cardputer Game OS individual game, rebuilt as a Waveshare touch-first campaign RPG.

## Campaign Loop

Patrol a strange park, log sightings, manage gear, and keep visitors calm.

Role: junior ranger, field folklorist, and night patrol lead.

## Waveshare Controls

- Tap `Play`, `Sheet`, `Quest`, `Lore`, or `Pack` tabs to change screens.
- On `Play`, tap one of the four action cards to advance the campaign day.
- Swipe up/down/left/right to move between screens.
- Hold BOOT to return to the launcher.
- Serial fallback: `act 1`, `act 2`, `act 3`, `act 4`, `screen play`, `screen sheet`, `screen quest`, `screen lore`, `screen pack`, `status`, `save`, `reset`, `home`.

## Campaign Data

- Compact campaign state saves to `/waveshare-os/cardputer-game-os/states/cryptid_park_ranger.txt`.
- Journal entries append to `/waveshare-os/cardputer-game-os/saves/cryptid_park_ranger.txt`.
- Existing journal files are not deleted by the campaign upgrade.

## Port Notes

This keeps the Game OS catalog entry installable as its own launcher `.bin` while adding stats, chapters, lore, inventory unlocks, and richer narrative outcomes without Cardputer keyboard assumptions.
