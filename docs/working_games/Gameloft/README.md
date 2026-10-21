# Gameloft MRE Games Testing Report

## Overview
This directory contains the testing results, screenshots, and metadata for all Gameloft and Gameloft-affiliated MediaTek MRE (`.vxp`) packages installed in `bin/Release/fs/e/mre/`.

### Methodology & Driving Mechanics
Gameloft games employ a multi-stage startup sequence before presenting the interactive title screen or main menu:
1. **Language Selection Menu**: Displays "English", "Français", etc. on a black screen. Requires `VM_KEY_OK` (`-5`) or `VM_KEY_NUM5` (`53`).
2. **Audio/Sound Prompt**: Prompts "Enable sound? [Yes] [No]". Requires Left Softkey (`VM_KEY_LEFT_SOFTKEY = -6`) or `VM_KEY_OK` (`-5`).
3. **Publisher / IP Logo**: E.g., Marvel Comics, Ubisoft, Sony Pictures, Columbia Pictures (timed for 2–3 seconds).
4. **Developer Logo**: "Gameloft presents" / animated logo (timed for 2–3 seconds).
5. **Title Screen / Main Menu**: Full interactive game title and menus.

To capture genuine **in-game** screens rather than early splash screens or language selection prompts, each game was driven through a 14–15 second simulation loop with timed key events (`OK`, `Key 5`, `Left Softkey`, and center touch taps).

---

## Results Summary

* **Total Gameloft Packages Tested**: 30
* **Successfully Entered In-Game Menus / Screens (`RunSuccess/`)**: **30 (100%)**
* **Crashes / Unhandled Faults (`RunFail/`)**: **0**

---

## Game Catalog & In-Game Screenshots

| # | VXP Filename | Title (`VM_CE_INFO_NAME`) | Developer | Version | RAM Req | In-Game Screenshot |
| :- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | `Asphalt.vxp` | Asphalt | GameLoft | 0 | 1200 KB | [Asphalt.png](RunSuccess/Asphalt.png) |
| 2 | `AssassinCreed3.vxp` | AssassinCreed3 | Gameloft | 0.3.0 | 1500 KB | [AssassinCreed3.png](RunSuccess/AssassinCreed3.png) |
| 3 | `Block Breaker 3.vxp` | BlockBreaker3 | Gameloft | 0 | 800 KB | [Block Breaker 3.png](RunSuccess/Block%20Breaker%203.png) |
| 4 | `Brain Challenge 3 240x320.vxp` | BrainChallenge3 | Gameloft | 0 | 1500 KB | [Brain Challenge 3 240x320.png](RunSuccess/Brain%20Challenge%203%20240x320.png) |
| 5 | `Brain Challenge 3 320x480.vxp` | BrainChallenge3 | Gameloft | 3.0.1 | 1500 KB | [Brain Challenge 3 320x480.png](RunSuccess/Brain%20Challenge%203%20320x480.png) |
| 6 | `Brain challenge 3.vxp` | BrainChallenge3 | Gameloft | 0 | 1500 KB | [Brain challenge 3.png](RunSuccess/Brain%20challenge%203.png) |
| 7 | `BrainChallenge3 (Brain Chalange 3).vxp` | BrainChallenge3 | Gameloft | 3.0.1 | 1500 KB | [BrainChallenge3.png](RunSuccess/BrainChallenge3%20(Brain%20Chalange%203).png) |
| 8 | `Bubble Frenzy.vxp` | Bubble Frenzy | Baltoro / Gameloft | 2.0.1 | 1500 KB | [Bubble Frenzy.png](RunSuccess/Bubble%20Frenzy.png) |
| 9 | `BubbleBash2.vxp` | BubbleBash2 | Gameloft | 3.0.1 | 1500 KB | [BubbleBash2.png](RunSuccess/BubbleBash2.png) |
| 10 | `Cricket 2016.vxp` | Cricket 2016 | Gameloft | 1.0.0 | 2500 KB | [Cricket 2016.png](RunSuccess/Cricket%202016.png) |
| 11 | `DangerDash.vxp` | DangerDash | Gameloft | 1.0.1 | 2500 KB | [DangerDash.png](RunSuccess/DangerDash.png) |
| 12 | `DiamondTwister2.vxp` | DiamondTwister2 | Gameloft | 1.0.2 | 1500 KB | [DiamondTwister2.png](RunSuccess/DiamondTwister2.png) |
| 13 | `GTRacing2.vxp` | GTRacing2 | Gameloft | 1.0.1 | 1500 KB | [GTRacing2.png](RunSuccess/GTRacing2.png) |
| 14 | `Gameloft Live.vxp` | Shop | Gameloft | 1.0.0 | 1500 KB | [Gameloft Live.png](RunSuccess/Gameloft%20Live.png) |
| 15 | `GamesAndApps.vxp` | GamesAndApps | Gameloft | 1.0.0 | 1500 KB | [GamesAndApps.png](RunSuccess/GamesAndApps.png) |
| 16 | `GangstarRio.vxp` | GangstarRio | Gameloft | 1.0.0 | 1500 KB | [GangstarRio.png](RunSuccess/GangstarRio.png) |
| 17 | `GoKartmania 2.vxp` | GoKartmania 2 | Baltoro / Gameloft | 2.0.1 | 1500 KB | [GoKartmania 2.png](RunSuccess/GoKartmania%202.png) |
| 18 | `Little Big City Mobile Freemium.vxp` | LittleBigCity | Gameloft | 0.2.0 | 1500 KB | [Little Big City Mobile Freemium.png](RunSuccess/Little%20Big%20City%20Mobile%20Freemium.png) |
| 19 | `Little Big City demo.vxp` | LittleBigCity | Gameloft | 1.0.0 | 1500 KB | [Little Big City demo.png](RunSuccess/Little%20Big%20City%20demo.png) |
| 20 | `MODERN COMBAT 2 240x320.vxp` | ModernCombat2 | Gameloft | 1.0.0 | 1500 KB | [MODERN COMBAT 2 240x320.png](RunSuccess/MODERN%20COMBAT%202%20240x320.png) |
| 21 | `Messenger.vxp` | Messenger | MediaTek / Gameloft | 1.0.0 | 2500 KB | [Messenger.png](RunSuccess/Messenger.png) |
| 22 | `MidnightPool3.vxp` | MidnightPool3 | Gameloft | 1.0.1 | 1500 KB | [MidnightPool3.png](RunSuccess/MidnightPool3.png) |
| 23 | `N.O.V.A.3 240x320.vxp` | NOVA3 | Gameloft | 1.0.0 | 1500 KB | [N.O.V.A.3 240x320.png](RunSuccess/N.O.V.A.3%20240x320.png) |
| 24 | `Real Football 2012 240x320.vxp` | RF2012 | Gameloft | 1.0.0 | 1500 KB | [Real Football 2012 240x320.png](RunSuccess/Real%20Football%202012%20240x320.png) |
| 25 | `Shop (Gameloft store live).vxp` | Shop | Gameloft | 1.0.0 | 1500 KB | [Shop (Gameloft store live).png](RunSuccess/Shop%20(Gameloft%20store%20live).png) |
| 26 | `Skateboard.vxp` | Skateboard | Gameloft | 1.0.0 | 1500 KB | [Skateboard.png](RunSuccess/Skateboard.png) |
| 27 | `SubwaySurfers.vxp` | SubwaySurfers | Gameloft | 1.0.0 | 2500 KB | [SubwaySurfers.png](RunSuccess/SubwaySurfers.png) |
| 28 | `The Amazing Spider-Man (Demo).vxp` | Spiderman4 | Gameloft | 1.0.0 | 1500 KB | [The Amazing Spider-Man (Demo).png](RunSuccess/The%20Amazing%20Spider-Man%20(Demo).png) |
| 29 | `The Avengers (1.0.0).vxp` | The Avengers | Gameloft | 1.0.0 | 1500 KB | [The Avengers (1.0.0).png](RunSuccess/The%20Avengers%20(1.0.0).png) |
| 30 | `The Avengers (3.0.0).vxp` | The Avengers | Gameloft | 3.0.0 | 1500 KB | [The Avengers (3.0.0).png](RunSuccess/The%20Avengers%20(3.0.0).png) |
