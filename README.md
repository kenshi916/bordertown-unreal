# BorderTown

Unreal Engine 5.8 FPS game in development, with BorderTown, Mexico and Carnival environments, Oskar character integration, selectable weapon loadouts, training bots, health/shield systems and first-person weapon work.

This repository contains the project's custom C++ source, configuration and weapon integration/QA helpers. **It is a source snapshot, not a standalone playable download.** The current game depends on separately acquired Unreal/Fab content and local weapon exports.

## Included

- `Source/BorderTown`: combat HUD/vitals, training bots, doors and game menu integration.
- `Source/BackstageDepot`: loadout/depot interface and character/pack preview code.
- `Config` and `BorderTown.uproject`: project configuration, with local identifiers and development-server credentials removed.
- `work/arsenal`: import helpers, independent weapon animation layers, interruption cleanup, rendered gameplay checks and guarded loadout promotion.
- `docs/ASSETS.md`: content dependencies and weapon source attribution.

## Local setup

1. Install Unreal Engine 5.8 and its supported Windows C++ build toolchain.
2. Acquire the project's FPS template, Oskar character and environment packs through your own accounts. Restore their content into the expected package paths listed in [the dependency notes](docs/ASSETS.md).
3. Generate project files for `BorderTown.uproject` and build the `BorderTownEditor` target for Development Editor / Win64.
4. Open the project. Maps, Blueprint gameplay and model assets must be supplied locally before the full game or weapon QA can run.

The custom source is separated from third-party content so the public repository does not redistribute the purchased template, character, animations or environment packs. Unreal Engine itself is not included. No downloadable build is published here yet.

## Weapon workflow

Imports use owned folders under `/Game/BorderTownWeapons/Arsenal/<Weapon>` and preserve the shared mannequin skeleton. Hand and weapon clips are checked at 60 Hz, materials are mapped explicitly, and the template's procedural ADS is verified in the running game. An imported mesh alone is not considered a playable weapon.

Rendered checks cover aiming, firing, empty and tactical reloads, magazine/bolt reset, switching, and interrupted reload recovery. Visual contact review is still required before adding a gun to saved loadouts. The checks currently exercise a local player; they do not certify multiplayer replication.

For a separately licensed working project, set `BORDERTOWN_PROJECT` to its directory before running the Python helpers inside Unreal. Use an isolated Unreal `-UserDir` and `-SaveToUserDir` for tests. `verify_gameplay.py` requires an explicit `-ExpansionGun=<Weapon>` and prepared QA assets. Export contracts, source FBXs, screenshots and private save profiles are intentionally not included.

## Development status

The local project is actively testing new weapon models. MP7, PGM, M1911 and knife integrations exist; the new 9mm/Talon pistols have passed local gameplay checks. UZI and AR15 are undergoing aimed-fire continuity corrections. MP5, Barrett and other downloaded guns are undergoing integration. This repository does not claim every download is already playable or that visual polish is finished.

## Rights

No open-source license has been assigned to the project's original code yet. Third-party dependencies retain their own licenses and are obtained separately. Public visibility does not change those licenses.
