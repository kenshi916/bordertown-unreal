# Asset dependencies

The public repository contains no Unreal `.uasset`/`.umap`, source model, texture, donor animation or downloaded archive. A complete game build requires the separately licensed content used by the private working project.

## Expected package roots

- `/Game/FPS_Controller`: the installed FPS controller/template and its animation assets. The working project uses this package layout; an arbitrary template version is not guaranteed compatible.
- `/Game/MetaHumans` and the installed Oskar assets: character/glove and retarget dependencies.
- `/Game/BorderTown`, `/Game/Carnival`, `/Game/BackstageDepot`: project maps, their dependencies and Blueprint integration.
- `/Game/BorderTownWeapons`: locally imported and validated weapon content.

These paths are package references, not download entitlements. Marketplace content and animations must be acquired through their publishers. The public source alone cannot reconstruct the authored maps or Blueprint catalog.

## Downloaded weapon sources

The following source metadata was recorded during asset inspection. Models are referenced here for attribution and are not redistributed by this repository. Local fit/rig/material/animation adaptation is in progress; this table is not a playable-status list.

| Model | Creator | Source license |
| --- | --- | --- |
| [AR-15](https://sketchfab.com/3d-models/ar-15-33e225811b404192b89dcd4096603fbe) | [wafla](https://sketchfab.com/wafla) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [KRISS VECTOR](https://sketchfab.com/3d-models/kriss-vector-3916980e5d564fc9bb1971c9d496bdd5) | [carolineblueeyes](https://sketchfab.com/carolineblueeyes) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [H&K MP7 A1](https://sketchfab.com/3d-models/hk-mp7-a1-27cd4b48fb14451c86adad4c559082d6) | [Steve Henry](https://sketchfab.com/Trinitite_Studio) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [MCX-SPEAR LT 5.56 11.5" SBR](https://sketchfab.com/3d-models/mcx-spear-lt-556-115-sbr-1ec6db3e7e234ed7bacb24cc69c85fe5) | [blarg](https://sketchfab.com/blarg1) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [Honey Badger](https://sketchfab.com/3d-models/honey-badger-0328770df6fb4496ace40192b9d3793f) | [bagusmars](https://sketchfab.com/bgsmars) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [Barrett M82 A1](https://sketchfab.com/3d-models/barrett-m82-a1-499195fd926c4016ae5aead4b9e33fb2) | [Gintoki1234](https://sketchfab.com/Gintoki1234) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [MP5 Submachine Gun](https://sketchfab.com/3d-models/mp5-submachine-gun-a73b61932a0e4eecb5db5c63c158aa24) | [Rotuma](https://sketchfab.com/Rotuma) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [sniper animated](https://sketchfab.com/3d-models/sniper-animated-d6b5bccd148540f0b722d05998c9af38) | [DJMaesen](https://sketchfab.com/bumstrum) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [sniper animated](https://sketchfab.com/3d-models/sniper-animated-b48999a250b2433da59f705c371a49b2) | [DJMaesen](https://sketchfab.com/bumstrum) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [pistol 9mm](https://sketchfab.com/3d-models/pistol-9mm-4477cfe3f4b342f4b5ebd3090b6b902f) | [DJMaesen](https://sketchfab.com/bumstrum) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
| [animated pistol](https://sketchfab.com/3d-models/animated-pistol-30d1b612b2334a8294b02692b8c10cf1) | [DJMaesen](https://sketchfab.com/bumstrum) | [CC Attribution](http://creativecommons.org/licenses/by/4.0/) |
