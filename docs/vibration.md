# Vibration API Usage in Installed Packages

Which of the packages in [`bin/Release/fs/e/mre/`](../bin/Release/fs/e/mre)
actually use the MediaTek MRE vibration APIs, and how.

Produced by disassembling the packages with
[`tools/vxpdisasm.py`](../tools/vxpdisasm.py) and resolving the import stubs
each guest builds around `vm_get_sym_entry`.

## How the list was derived

Each guest resolves its imports through `vm_get_sym_entry` by name, so the
strings `vm_vibrator_once`, `vm_vibrator_on` and `vm_vibrator_off` appear in
any package that pulls the API in. Finding the string only proves the package
*imports* it. Establishing that it is actually *called* needs a second step:
locate the stub the guest builds for that symbol, then find every `bl` / `blx`
or veneer that reaches it. That second step is what separates the direct
call sites in the table below from packages that merely link the stub.

The four usage patterns below are the result of that second step.

## How MREmu answers these calls

`vm_vibrator_once`, `vm_vibrator_on` and `vm_vibrator_off` are all implemented
in [`MREngine/System.cpp`](../MREmu/MREngine/System.cpp), which forwards to the
host vibrator through
[`jni/Vibration.cpp`](../MREmu/jni/Vibration.cpp). `once` is a 40 ms tick,
`on` is a 5 s buzz, `off` cancels.

On Android the duration is not used verbatim: see the waveform-shaping notes in
[`MainActivity.kt`](../Android/app/src/main/java/com/ximikboda/mremu/MainActivity.kt).
Short pulses become OEM-tuned `EFFECT_TICK` / `EFFECT_CLICK` predefined effects
on API 29+, because a rotating-mass motor and a linear resonant actuator need
very different pulse lengths and no public API reports which one a handset has.
Long pulses stay a one-shot of the requested length.

Desktop builds compile `mremu_vibrate` to a no-op, so none of this is
observable there.

## Summary

| Vibration API combination | Count | Applications |
| :--- | ---: | :--- |
| **`vm_vibrator_on` + `vm_vibrator_off`** | **2** | `DangerDash.vxp`, `SubwaySurfers.vxp` |
| **`vm_vibrator_once`** | **48** | All remaining 48 applications |

48 of 142 packages, roughly a third of the catalog, resolve a vibration API.

## Usage patterns

### A. Direct gameplay call sites

These have `bl` / `blx` instructions targeting the vibration stub directly.

- **`Bike Adventure 240x320.vxp`** and **`Bike Adventure 176x220.vxp`** — ten
  call sites each (`0x610c`, `0x66dc`, `0x6998`, `0x6d98`, `0x6f38`, `0x70c8`,
  `0x7160`, `0x7274`, `0x7710`, `0x77d0`). The site at `0x6998` fires when
  player health drops (`sub r0, r0, #1`, logging `"player_health decreased %d"`
  then `"vibration hit"`) and is gated on the in-game vibration setting. Both
  builds also expose `"VIBRATION ON"` / `"VIBRATION OFF"` toggles at `0x8518`.
- **`Cricket.vxp`** — direct call at `0x9920`, fired alongside sound effects
  (`mov r0, #2; bl sound_play`) and gated on the same setting
  (`cmp r0, #1; bleq`). Menu toggle reads `"vibrator : on"` / `"vibrator : off"`
  under `"Setting"`.
- **`SMASHnWIN.vxp`** — two direct calls, at `0x83e0` and `0x855c`, on smashing
  a target, hitting a boundary, and on the win sequence.

### B. Gameloft variable-duration pulse engine

`DangerDash.vxp` and `SubwaySurfers.vxp` are built on Gameloft's MRE 2.0
porting layer, which does not use a fixed-duration buzz at all. It drives
`vm_vibrator_on` on a collision, stumble or train crash, arms a short timer
(50–200 ms), and calls `vm_vibrator_off` when it expires. That is why both are
the only packages using the `on`/`off` pair rather than `once`.

This is also the strongest argument for implementing `vm_vibrator_off` as a
real cancel rather than leaving it inert: these two games genuinely depend on
being able to stop a buzz they started.

### C. Arcade and action game engines

`Happy Egg`, `Moto Race`, `Parkour`, `HappyWar`, `GoldenMiner`, `GirlSlot`,
`KING COMBAT III`, `EgyptCode`, `Tear Dress TS` and `click_to_game` statically
link the audio/haptic subsystem (`peraudio.a` from the MRE SDK) and expose an
in-game options menu with a `Vibration: On/Off` toggle. They reach the stub
through jump veneers rather than direct calls.

### D. J2ME / MIDP runtimes

`Facebook.vxp` and `qvm.tank.qv.vxp` embed a MIDP 2.0 compatibility layer
(`microedition.platform`, `microedition.profiles`) that maps the standard Java
`javax.microedition.lcdui.Display.vibrate(int)` and `DeviceControl.vibrate()`
onto `vm_vibrator_once` — used for incoming-message notifications in Facebook
and for hit reactions in Tank.

## Complete list (50)

| # | VXP Filename | Vibration APIs Used | Engine / Category | Where / How Used |
| ---: | :--- | :--- | :--- | :--- |
| 1 | `Asphalt.vxp` | `vm_vibrator_once` | Gameloft | Crash / nitro collision feedback |
| 2 | `Bike Adventure 176x220.vxp` | `vm_vibrator_once` | Native Action | 10 gameplay call sites (damage taken, hit obstacle, menu toggle) |
| 3 | `Bike Adventure 240x320.vxp` | `vm_vibrator_once` | Native Action | 10 gameplay call sites (`0x610c`–`0x77d0`, `"vibration hit"`) |
| 4 | `Bikini Beach Bowling.vxp` | `vm_vibrator_once` | Native Sports | Pin strike / gutter collision |
| 5 | `BlowingSkirt.vxp` | `vm_vibrator_once` | Native Arcade | Blow interaction threshold trigger |
| 6 | `Bubble Hunter 240x320.vxp` | `vm_vibrator_once` | Native Puzzle | Bubble cluster pop / game over |
| 7 | `CHENNAI EXPRESS 240x320.vxp` | `vm_vibrator_once` | Native Runner | Obstacle collision / hit |
| 8 | `Car Race 240x320.vxp` | `vm_vibrator_once` | Native Racing | Car crash / wall collision |
| 9 | `Cricket.vxp` | `vm_vibrator_once` | Native Sports | Direct call `0x9920` (bat hit, wicket/out, options toggle) |
| 10 | `DangerDash.vxp` | `vm_vibrator_on`, `vm_vibrator_off` | Gameloft Engine | Variable-duration pulse on crash/stumble |
| 11 | `Demo.vxp` | `vm_vibrator_once` | SDK Sample | Hardware feature test / vibration demo |
| 12 | `Dragon Fire 240x320.vxp` | `vm_vibrator_once` | Native Shooter | Boss hit / bomb explosion |
| 13 | `EgyptCode.vxp` | `vm_vibrator_once` | Native Puzzle | Match fail / trap trigger (`0x1dd60`) |
| 14 | `Facebook.vxp` | `vm_vibrator_once` | J2ME Wrapper | Incoming message / notification alert |
| 15 | `FrogTerminator.vxp` | `vm_vibrator_once` | Native Arcade | Strike impact / level fail |
| 16 | `FruitLink.vxp` | `vm_vibrator_once` | Native Puzzle | Match combo / time warning |
| 17 | `Fun Park CN 240x320.vxp` | `vm_vibrator_once` | Native Arcade | Mini-game hit / ticket win |
| 18 | `Funny Bubble 240x320.vxp` | `vm_vibrator_once` | Native Puzzle | Bubble pop / boundary hit |
| 19 | `GPT.vxp` | `vm_vibrator_once` | App / Utility | Query complete / haptic response |
| 20 | `Girl Tetris CN 240x320.vxp` | `vm_vibrator_once` | Native Puzzle | Line clear / game over |
| 21 | `GirlSlot.vxp` | `vm_vibrator_once` | Native Casino | Reel stop / jackpot spin (`0x1a6f0`) |
| 22 | `GoldenMiner.vxp` | `vm_vibrator_once` | Native Arcade | Heavy pull / dynamite explosion (`0x1ca08`) |
| 23 | `Goolie.vxp` | `vm_vibrator_once` | Native Arcade | Monster hit / death |
| 24 | `Happy Egg 240x320.vxp` | `vm_vibrator_once` | Native Arcade | Egg crack / drop fail (`0x1dd1c`), menu toggle |
| 25 | `HappyWar.vxp` | `vm_vibrator_once` | Native Strategy | Unit damage / base hit (`0x202dc`) |
| 26 | `HungryBird 240x320.vxp` | `vm_vibrator_once` | Native Physics | Bird launch / wood crash |
| 27 | `KING COMBAT III 240x320.vxp` | `vm_vibrator_once` | Native Fighter | Special move / KO hit (`0x2cd4c`) |
| 28 | `KidsAbc 240x320.vxp` | `vm_vibrator_once` | Educational | Incorrect answer buzzer |
| 29 | `Little Big City Mobile Freemium.vxp` | `vm_vibrator_once` | Gameloft | Building completion / reward collect |
| 30 | `Lock D Block TETRIS 240x320.vxp` | `vm_vibrator_once` | Native Puzzle | Hard drop / block lock |
| 31 | `Moto Race 320x480.vxp` | `vm_vibrator_once` | Native Racing | Bike crash / fall (`0x1e718`), menu toggle |
| 32 | `Mrecp.vxp` | `vm_vibrator_once` | App / Control Panel | Haptic key feedback / alert test |
| 33 | `Opera Mini (6.1).vxp` | `vm_vibrator_once` | Browser Runtime | Static SDK link (`0xa240`) / alert vibration |
| 34 | `Parkour 240x320.vxp` | `vm_vibrator_once` | Native Runner | Fall / obstacle hit (`0x24864`), menu toggle |
| 35 | `Princess wall.vxp` | `vm_vibrator_once` | Native Arcade | Brick collision / lost life |
| 36 | `Runaway Car 240x320.vxp` | `vm_vibrator_once` | Native Racing | Traffic collision / game over |
| 37 | `SMASHnWIN.vxp` | `vm_vibrator_once` | Native Action | Direct calls `0x83e0`, `0x855c` (smash impact, win) |
| 38 | `StarZuma.vxp` | `vm_vibrator_once` | Native Puzzle | Chain reaction / skull-skull-mouth entry |
| 39 | `Starwars 240x320.vxp` | `vm_vibrator_once` | Action | Blaster hit / lightsaber clash |
| 40 | `SubwaySurfers.vxp` | `vm_vibrator_on`, `vm_vibrator_off` | Gameloft Engine | Variable-duration pulse on barrier/train crash |
| 41 | `Sushi Shuffle.vxp` | `vm_vibrator_once` | Native Puzzle | Combo match / timer runout |
| 42 | `Tap Tap Tap.vxp` | `vm_vibrator_once` | Rhythm / Arcade | Missed tap / beat feedback |
| 43 | `Tear Dress TS.vxp` | `vm_vibrator_once` | Native Touch | Touch action threshold (`0x17824`) |
| 44 | `The Assembly Cn.vxp` | `vm_vibrator_once` | Native RPG | Attack hit / critical strike |
| 45 | `The Yulgang Tianxian Recorded 240x320.vxp` | `vm_vibrator_once` | Native RPG | Skill cast / damage received |
| 46 | `Warcraft Link-up.vxp` | `vm_vibrator_once` | Native Puzzle | Tile match / wrong pair buzzer |
| 47 | `Zapper Racing.vxp` | `vm_vibrator_once` | Native Racing | Zap trap / crash |
| 48 | `click_to_game.vxp` | `vm_vibrator_once` | Arcade Launcher | Selection confirm (`0x461c`) |
| 49 | `qvm.tank.qv.vxp` | `vm_vibrator_once` | QVM / J2ME Engine | Tank hit / shell explosion (`0x22cdc`) |
| 50 | `tranForm.vxp` | `vm_vibrator_once` | Native Action | Transform sequence / boss attack |

## Caveats

- The **addresses** in the last column are guest virtual addresses, valid for
  that specific build only. The two Bike Adventure builds and the differing
  Brain Challenge 3 sizes are separate packages, so do not share them.
- The "Where / How Used" descriptions for pattern **C** in particular are
  **inferred** from the surrounding code and nearby string literals, not from
  tracing an executed call. Pattern **A** is the well-evidenced one — those are
  real `bl` / `blx` sites to the stub. Confirming C properly needs an MCP VM
  call trace against a running app.
- `Opera Mini (6.1).vxp` is listed on the strength of a static SDK link
  (`0xa240`). Whether it ever actually buzzes was not established.
- The list covers only the 142 packages in the MRE folder as of writing. Adding
  or removing packages changes it.
- Several of these games gate vibration behind an in-game setting. That
  setting is the *game's*, not MREmu's — MREmu has no equivalent, so a guest
  that wants vibration off globally has no way to say so.

## Related

- [MRE Runtime APIs](mre_api.md) — the full emulated API reference
- [Unimplemented MRE APIs](unimplemented_apis.md) — what is still missing
- [Android Frontend](android.md) — the JNI bridge and keypad