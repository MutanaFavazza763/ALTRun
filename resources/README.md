# Runtime resources

Files copied next to `ALTRun.exe` at build time (see the POST_BUILD step in
`CMakeLists.txt`).

Planned contents (mirrors the original `Bin/` folder):

| Path | Purpose |
| --- | --- |
| `res/skin/*.png` | Launcher background skins (`BGFileName` in the ini) |
| `res/lang/*.lang` | Language files (`Lang` in the ini) |
| `res/Popup.wav` | The "popup" notification sound (`PlayPopupNotify`) |
| `res/ALTRun.ico` | Tray / window icon |

Nothing here is required at runtime: the launcher falls back to a solid dark
background, and all three UI locales (`zh_cn`, `zh_hk`, `en_us`) are built into
the executable.  External `.lang` files are not loaded.
