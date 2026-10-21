# API implementation TODO

What to implement next in the MRE API surface, and what not to bother with.

This is the prioritised companion to
[`unimplemented_apis.md`](unimplemented_apis.md), which records *what* is
missing. This records *what is worth doing about it* - and the answer is not
"the 628 unimplemented APIs in order".

## How the ranking was derived

```python
python tools/apisurvey.py --demand      # re-run the analysis
python tools/apisurvey.py --check       # does the missing-list still match?
```

Demand comes from the installed packages, not from guesswork: every `vm_*`
symbol string in `bin/Release/fs/e/mre/*.vxp` is cross-referenced against the
unimplemented set. Three corrections matter, because without them the numbers
are misleading:

- **Compressed packages are inflated first.** Four of the installed packages
  are `ads_compressed`; a raw string scan finds *nothing* in them. Inflating
  the zlib streams turns Facebook Messenger from 0 symbols into 145, Gameloft
  Live 87, Games and Apps 106, Spider-Man 100. Those four are invisible
  otherwise - and they are exactly the apps that had boot trouble.
- **Duplicate builds count once.** PeanutVXP ships at three sizes and Opera
  Mini 4.4 at two. Counting the builds separately would have tripled
  PeanutVXP's apparent demand and made the top of the list meaningless.
- **`vm_main` is excluded.** It is the application entry point, not an API the
  guest resolves. That fix corrected
  [`unimplemented_apis.md`](unimplemented_apis.md), which had been counting it
  (along with `vm_sprintf` and friends, which `ARModule` resolves after all).

### The shape of demand

| | Count |
| --- | ---: |
| Apps measured | 139 |
| APIs unimplemented | 628 |
| Wanted by at least one app | 375 |
| **Wanted by nobody measured** | **253** |
| Wanted by 2+ apps | 295 |
| Wanted by 3+ apps | 272 |

Two things follow, and they shape everything below.

**More than a third of the backlog is dead weight.** 253 APIs are wanted by no
measured app - camera, Bluetooth, video, payment, GPS, telephony. That is
hardware an emulator does not have. It is not unfinished work; it is work
that should never be done.

**Demand is now broad, not concentrated.** The previous survey (15 apps) was
dominated by Opera Mini 6.1 alone (214 of 304 demands). With 139 apps
measured, the picture changed: 295 APIs are wanted by 2+ apps and 272 by 3+.
Popularity is now a strong signal. The HTTP and SMS clusters each reach
50-70 apps - more than half the catalog. The old Tier 1 (trivial one-liners
like `vm_file_rmdir`) is no longer in the demanded set at all.

### Caveats

- A `vm_*` **string** in a binary is a proxy for what the app resolves at
  start-up, not proof that it calls it. These counts over-count.
- This measures **only the apps installed here**. It says nothing about the
  wider VXP ecosystem, and the MRE folder contents shift between runs.
- A NULL symbol is usually survivable - apps feature-detect. Rank is by
  *reach*, not by *severity*.

---

## Tier 1 - do these now

Small-to-medium, contained, and each unlocks a large fraction of the catalog.
Effort is per item including tests.

- [ ] **Async HTTP cluster** (`vm_asyn_http_req`, `vm_get_asyn_http_req_handle`,
      `vm_cancel_asyn_http_req`, `vm_enable_proxy`, `vm_http_free_handle`,
      `vm_cancel_all_http_sessions`, `vm_url_encode_gb2312`) - 42-71 apps each,
      the widest reach in the entire backlog. MREmu already has an Asio socket
      stack and `vm_tcp_connect`, so this is mostly request lifecycle and
      marshalling callbacks back to the guest thread - the awkward part, not
      the network. *Effort: large. Highest reach per unit of work.*
- [ ] **`vm_midi_resume`** - 69 apps. MIDI playback resume. The audio
      subsystem already exists; this is a state toggle. *Effort: small.*
- [ ] **`vm_pmng_state`** - 56 apps. Process manager state query. *Effort:
      small.*
- [ ] **`vm_memcpy`, `vm_lower_case`, `vm_string_equals_ignore_case`** - 42-53
      apps. Stdlib string/memory helpers. *Effort: trivial.*
- [ ] **`vm_graphic_polygon`** - 60 apps. Polygon drawing primitive.
      *Effort: small.*
- [ ] **`vm_send_sms`** - 59 apps. *Reading* a fake message store is
      defensible; *sending* is not - see Tier 3. Register and do nothing, or
      return "no modem". *Effort: trivial, but read the note below first.*
- [ ] **`vm_graphic_set_alpha_blending_layer`** - 52 apps. Note `struct layer`
      in [`Graphic.h`](../MREmu/MREngine/Graphic.h) currently has no opacity or
      blend field, so this is not a one-liner - it means adding the fields and
      honouring them in `Blt.cpp`. Contained, but real work. *Effort:
      small-to-medium.*
- [ ] **`vm_graphic_draw_image_from_memory`** - 48 apps. Blit from a memory
      buffer instead of a file. *Effort: small.*
- [ ] **`vm_graphic_draw_resized_image_from_memory`** - 47 apps. Same as above
      with scaling. *Effort: small.*
- [ ] **`vm_audio_bytes_duration`** - 46 apps. Duration of an audio buffer in
      ms. *Effort: small.*
- [ ] **`vm_sm_get_func_entry`, `vm_sm_load`, `vm_sm_unload`** - 45 apps each.
      Script-module management. *Effort: medium.*
- [ ] **SMS read store** (`vm_sms_get_sc_address_syn`, `vm_sms_read_msg`,
      `vm_sms_get_msg_id`, `vm_sms_get_box_size`, `vm_sms_delete_msg`) - 47-48
      apps. Reading a local store is reasonable; *sending* is not - see Tier 3.
      *Effort: medium.*
- [ ] **`vm_graphic_load_gif_resized_by_percent`** - 43 apps. GIF decode with
      scaling. *Effort: medium.*
- [ ] **`vm_audio_play_file`** - 43 apps. Play an audio file from disk.
      *Effort: small.*
- [ ] **`vm_get_battery_level`** - 42 apps. A fixed percentage is a
      legitimate answer on a desktop build. *Effort: trivial.*

## Tier 2 - cluster unlocks

Bigger, but each is a coherent family that several apps want at once. Do them
whole or not at all.

- [ ] **Audio family** (`vmmm.h`) - 23 APIs demanded, spanning MIDI, WAV
      playback, duration queries, and key-tone control. The audio subsystem
      exists; this is filling in the VM-facing wrappers. *Effort: medium.*
- [ ] **Graphics old API** (`vmgfxold.h`) - 16 APIs demanded, including
      `vm_dd_img_flood` and `vm_dd_active_layer`. Legacy drawing primitives
      that many apps still resolve. *Effort: medium.*
- [ ] **Contacts** (`vmcontact.h`) - 14 APIs demanded, including
      `vm_phb_get_used_contacts`. Needs a small synthetic phonebook.
      *Effort: medium.*
- [ ] **Camera** (`vmcamera.h`) - 13 APIs demanded. There is no camera on a
      desktop build, so these are *register and do nothing* - which is a
      legitimate answer here, unlike the inert stubs in the companion
      document. *Effort: small (stubs), but read the note below first.*
- [ ] **Text editor (`vm_editor_*`)** - the clearest single unblock for
      Facebook Messenger. `SystemTextBox` is already a complete text box with
      cursor, selection, mouse handling and scrolling, wired to
      `vm_input_text*`; `vm_editor_*` is the same thing as a caller-owned
      control rather than a modal prompt. Check whether Messenger still
      demands it under the new package set. *Effort: large, but most of the
      hard work already exists.*
- [ ] **Layer compositing controls** - `vm_graphic_set_blt_layer`,
      `vm_graphic_set_layer_opacity`,
      `vm_graphic_set_alpha_blending_layer`. Note `struct layer` in
      [`Graphic.h`](../MREmu/MREngine/Graphic.h) currently has no opacity or
      blend field, so these are not one-liners - they mean adding the fields
      and honouring them in `Blt.cpp`. Contained, but real work.
      *Effort: small-to-medium.*
- [ ] **IO and storage** (`vmio.h`) - 15 APIs demanded, including
      `vm_storage_close`. File and storage helpers beyond the basic
      `vm_file_*` set. *Effort: medium.*
- [ ] **Script module management** (`vmsm.h`) - 11 APIs demanded. Module
      loading and entry-point resolution. *Effort: medium.*
- [ ] **Stdlib** (`vmstdlib.h`) - 21 APIs demanded. String, memory, and
      cache helpers. *Effort: medium.*

## Tier 3 - deliberately do not implement

Wanted by apps, but implementing them would be wrong. Recording the decision
is the point; otherwise these keep reappearing in every triage.

- [ ] **Outbound telephony and SMS send** - `vm_send_sms`, `vm_callout`,
      `vm_cancel_send_sms`. There is no modem. Reading a fake message store
      is defensible; pretending to send is not.
- [ ] **Camera (13), video and RTSP (15), GPS (8), Bluetooth** - real
      hardware emulation, no reach beyond the single app that asks for each,
      and it would consume the entire backlog. The 253 APIs nobody measured
      are mostly these.
- [ ] **Payment** (`vmpayment.h`) - 3 APIs, 2 apps. No payment backend.
- [ ] **`vm_update_check_version`, `vm_update_download`,
      `vm_update_update_file`, `vm_update_cancel`** - OTA self-update path.
      An emulator should *not* phone home or replace installed packages. If an
      app gates on these, the right move is a truthful "no update available"
      rather than a working updater.

## Tier 4 - only for a specific goal

- **Opera Mini 6.1's demands** span every subsystem (`vmmm` 20, `vmstdlib`
  20, `vmgraph` 20, `vmgfxold` 16, `vmhttp` 14, `vmvideo` 14, `vmcamera`
  13...). Chasing this list wholesale is not worth it - it is effectively
  "implement MRE". Pull individual items from it when a specific Opera
  feature is the actual goal.
- **Facebook Messenger's remaining demands** are the editor cluster above
  plus graphics and HTTP calls - most of which Tier 1 already covers.
- **Asphalt's 42 exclusive demands** are mostly graphics and HTTP helpers
  already in Tier 1.

## Refreshing this

The demand figures move with the contents of the MRE folder, so re-run
`tools/apisurvey.py --demand` after adding or removing packages and update
the counts above. The Tier 1 and Tier 2 reasoning holds regardless, because
it rests on which families apps ask for rather than on the exact totals.