# Tasks — ESP32-S3-Key BadUSB improvements

1. [x] Fix tab-switch bug (remove duplicate openTab)
2. [x] Make UI responsive for mobile
3. [x] Add "Live" typing tab (UI)
4. [x] Add `/api/live-type` firmware endpoint
5. [x] Fix LED commands for single blue LED + update Help
6. [x] Silent Startup: stealth HID attach/detach (firmware)
7. [x] Silent Startup: Settings toggle + endpoint (UI)
8. [x] Bug-hunt rounds 1-6: boot re-exec, boot-scripts stat, invisible boot preview, dup openTab, wrong active-tab, unhandled rejection
9. [x] Verify README roadmap vs code, update ✅/❌
10. [x] Cleanup build/, compile-verify (NO flash — user asleep)
11. [x] Bundled update package: `.espkg` format + firmware UpdateManager (SD files first, then OTA, progress)
12. [x] Python builder script `build_espkg.py`
13. [x] WebUI: Settings "Firmware Update" (file picker + upload progress bar + apply-status polling)
14. [x] Bug-hunt rounds 7-10: update/upload path, OTA safety, streaming, remaining fragility
15. [x] Final compile-verify (NO flash), commit, push to private esp32-s3-key repo
16. [x] Write SESSION_SUMMARY.md, then shutdown

---

For 11. Define `.espkg`: 6-byte magic `ESPKG\x01`, uint32 manifest length, JSON manifest `{version, sd:[{path,size}], fw:{size}}`, then payloads concatenated (SD files in listed order, then firmware last). Add `UpdateManager.cpp/.h`: `POST /api/update-package` streams the upload to `/update.espkg` on SD; on completion parse it, write each SD file (website first), then apply firmware via `Update.h` (OTA) and reboot. Track `updateProgress`/`updateStatus`; expose `GET /api/update-status`. Done when: firmware compiles and apply writes SD files then OTAs.

For 12. Write `tools/build_espkg.py`: args for website dir + firmware .bin, emits `firmware.espkg` in the format above with version + sizes + CRC32. Done when: it builds a valid `.espkg` and prints a summary.

For 13. In `index.html` Settings add "Firmware Update": `.espkg` file input, Update button, progress bar, status line. In `script.js` upload via `XMLHttpRequest` with `upload.onprogress` → progress bar, then poll `/api/update-status`; show reboot message. Done when: UI shows upload % then apply status.

For 14. Bug-hunt 4 more rounds focused on the update/upload path (partial upload, bad magic, truncated package, OTA failure), streaming buffers, remaining web/firmware fragility. Fix confirmed findings. Done when: 4 rounds reported, solid bugs fixed.

For 15. Compile-verify the final firmware (do NOT flash). Commit everything and push to the PRIVATE esp32-s3-key repo. Done when: compile exit 0 and push succeeds.

For 16. Write SESSION_SUMMARY.md (what was done, what remains to flash/test), then run shutdown-when-done as the very last action. Done when: summary saved and shutdown scheduled.

---

## Follow-up session (user testing feedback)

17. [x] Fix: Live typing not sending per-keystroke — was gated behind an off-by-default toggle. Now armed on load (toggle defaults ON, `● LIVE` status), verified in-browser: each keystroke + Enter + Ctrl-combo POSTs to /api/live-type.
18. [x] Fix: Silent Startup still made the Windows connect sound — USB was enumerating at boot then detaching. Now `USB.begin()` is DEFERRED entirely in silent mode (no boot enumeration, no sound); USB starts on first script/live-type via `ensureHidReady()`.
19. [x] Bug-hunt (10 fresh rounds) on the new HID/live code: fixed 2 Major (set-silent & HID_ATTACH called tud_connect on uninitialized USB after a silent boot) + 3 Minor (IME/dead-key guard, Send-All-Text >2KB, live-off release). 5 rounds clean.
20. [x] Re-compile + re-flash with all fixes; commit + push.
