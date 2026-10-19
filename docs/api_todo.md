# API implementation TODO

What to implement next in the MRE API surface, and what not to bother with.

This is the prioritised companion to
[`unimplemented_apis.md`](unimplemented_apis.md), which records *what* is
missing. This records *what is worth doing about it* - and the answer is not
"the 643 unimplemented APIs in order".

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
| Apps measured | 15 |
| APIs unimplemented | 643 |
| Wanted by at least one app | 304 |
| **Wanted by nobody measured** | **339** |
| Wanted by 2+ apps | 47 |
| Wanted by 3+ apps | 24 |

Two things follow, and they shape everything below.

**More than half the backlog is dead weight.** 339 APIs are wanted by no
measured app - camera, Bluetooth, video, payment, GPS, telephony. That is
hardware an emulator does not have. It is not unfinished work; it is work
that should never be done.

**The head of the list is dominated by one app.** Opera Mini 6.1 alone wants
214 of the 304. Facebook Messenger alone wants 32. Only 47 APIs are wanted by
more than one app, so "popularity" is a weak signal here and cluster
completeness is the stronger one: apps ask for a *family* (`vm_sms_*`,
`vm_editor_*`), and a family is worth doing together or not at all.

### Caveats

- A `vm_*` **string** in a binary is a proxy for what the app resolves at
  start-up, not proof that it calls it. These counts over-count.
- This measures **only the 15 apps installed here**. It says nothing about the
  wider VXP ecosystem, and the MRE folder contents shift between runs.
- A NULL symbol is usually survivable - apps feature-detect. Rank is by
  *reach*, not by *severity*.

---

## Tier 1 - do these now

Small, contained, and each unlocks several apps. Effort is per item including
tests.

- [ ] **`vm_file_rmdir`** - 8 apps, the widest reach in the entire backlog by
      a factor of two. `vm_file_delete` already does `fs::remove(path)` at
      [`IO.cpp:372`](../MREmu/MREngine/IO.cpp); this is `fs::remove_all` in
      the same shape. *Effort: trivial. Best ratio available.*
- [ ] **`vm_get_default_apn_info`** - 5 apps, mostly the Opera and Facebook
      family. A fixed APN string is a legitimate answer on a machine with no
      real bearer. *Effort: trivial.*
- [ ] **`vm_get_filename`, `vm_get_path`, `vm_get_default_folder_path`** - 2-3
      apps each. Path decomposition over the existing UCS2 helpers in
      `STDLib.cpp`. *Effort: trivial.*
- [ ] **`vm_get_mre_version`** - 2 apps. A version string constant. *Effort:
      trivial.*
- [ ] **`vm_get_multi_tags`** - 3 apps. `vm_get_vm_tag` already exists and
      already handles both the `@` ROM form and plain files; this is a loop
      over it. *Effort: small.*
- [ ] **`vm_kbd_set_mode`** - 3 apps. Store and return the mode.
      *Effort: trivial.*
- [ ] **`vm_get_string_width_height_ex`** - 2 apps. `Textout.cpp` already has
      both measurements for the plain case. *Effort: small.*
- [ ] **`vm_load_resource_from_file`** - 2 apps. `Resources.cpp` already
      resolves resources for the running app; this is the same lookup against
      a caller-supplied path. *Effort: small.*
- [ ] **`vm_vibrator_once`** - 4 apps. There is no vibrator on a desktop
      build, so this is *register it and do nothing* - which is a legitimate
      answer here, unlike the inert stubs in the companion document.
      *Effort: trivial, but read the note below first.*

## Tier 2 - cluster unlocks

Bigger, but each is a coherent family that several apps want at once. Do them
whole or not at all.

- [ ] **Asynchronous HTTP** - the single highest-value cluster. 5-6 apps.
      `vm_asyn_http_req`, `vm_get_asyn_http_req_handle`, `vm_http_free_handle`,
      `vm_cancel_asyn_http_req`, `vm_cancel_all_http_sessions`,
      `vm_enable_proxy`. MREmu already has an Asio socket stack and
      `vm_tcp_connect`, so this is mostly request lifecycle and marshalling
      callbacks back to the guest thread - the awkward part, not the network.
      *Effort: large. Highest reach per unit of work.*
- [ ] **Text editor (`vm_editor_*`)** - 23 APIs, and 23 of Facebook Messenger's
      51 demands. `SystemTextBox` is already a complete text box with cursor,
      selection, mouse handling and scrolling, wired to `vm_input_text*`;
      `vm_editor_*` is the same thing as a caller-owned control rather than a
      modal prompt. This is the clearest single unblock on the list.
      *Effort: large, but most of the hard work already exists.*
- [ ] **Layer compositing controls** - 2-3 apps, and cheap enough to matter.
      `vm_graphic_set_blt_layer`, `vm_graphic_set_layer_opacity`,
      `vm_graphic_set_alpha_blending_layer`. Note `struct layer` in
      [`Graphic.h`](../MREmu/MREngine/Graphic.h) currently has no opacity or
      blend field, so these are not one-liners - they mean adding the fields
      and honouring them in `Blt.cpp`. Contained, but real work.
      *Effort: small-to-medium.*
- [ ] **SMS read store** - 3-4 apps. `vm_sms_read_msg`, `vm_sms_get_msg_id`,
      `vm_sms_get_box_size`, `vm_sms_delete_msg`. Reading a local store is
      reasonable; *sending* is not - see Tier 3. *Effort: medium.*
- [ ] **Contacts** - 3 apps. `vm_phb_get_used_contacts`,
      `vm_phb_get_used_contacts_count`, `vm_phb_get_contact`,
      `vm_phb_get_contact_syn`. Needs a small synthetic phonebook.
      *Effort: medium.*

## Tier 3 - deliberately do not implement

Wanted by apps, but implementing them would be wrong. Recording the decision
is the point; otherwise these keep reappearing in every triage.

- [ ] **`vm_update_check_version`, `vm_update_download`,
      `vm_update_update_file`, `vm_update_cancel`** - 3 apps want the MediaTek
      OTA self-update path. An emulator should *not* phone home or replace
      installed packages. If an app gates on these, the right move is a
      truthful "no update available" rather than a working updater.
- [ ] **Outbound telephony and SMS send** - `vm_send_sms`, `vm_callout`,
      `vm_sms_launch`. There is no modem. Reading a fake message store is
      defensible; pretending to send is not.
- [ ] **Camera (36), video and RTSP (19), Bluetooth (33), payment, GPS** -
      one app each at most, most of that single app being Opera Mini 6.1.
      Real hardware emulation, no reach, and it would consume the entire
      backlog. The 339 APIs nobody measured are mostly these.

## Tier 4 - only for a specific goal

- **Opera Mini 6.1's 214 exclusive demands** span every subsystem
      (`vmmm` 20, `vmstdlib` 20, `vmgraph` 20, `vmgfxold` 16, `vmhttp` 14,
      `vmvideo` 14, `vmcamera` 13...). Chasing this list wholesale is not
      worth it - it is effectively "implement MRE". Pull individual items from
      it when a specific Opera feature is the actual goal.
- **Facebook Messenger's remaining 32** are the editor cluster above plus
      `vm_file_rmdir`, `vm_get_default_apn_info`, `vm_get_multi_tags`,
      `vm_get_default_folder_path` and four graphics calls - most of which
      Tier 1 already covers.

## Refreshing this

The demand figures move with the contents of the MRE folder, so re-run
`tools/apisurvey.py --demand` after adding or removing packages and update
the counts above. The Tier 1 and Tier 2 reasoning holds regardless, because it
rests on which families apps ask for rather than on the exact totals.