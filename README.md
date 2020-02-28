# NX-FPS

SaltyNX plugin that collects FPS data in Nintendo Switch games. You need my fork of SaltyNX installed.
https://github.com/masagrator/SaltyNX/releases

Put NX-FPS.elf to `/SaltySD/plugins`

Currently supported graphics APIs:
- NVN

When game is booted, plugin outputs two files:
```
/SaltySD/FPSoffset.hex
/SaltySD/FPSavgoffset.hex
```

There are stored pointers where you can find PFPS (FPSoffset.hex) and FPS (FPSavgoffset.hex).
If files are already there, they are rewritten by new pointers with each new game boot.

To show it on display, you can use Status Monitor Overlay >=0.4
https://github.com/masagrator/Status-Monitor-Overlay

>WARNING

Plugin brings some instability to boot process for some games. It is recommended to not close game before ~10 seconds have passed from showing Nintendo logo, otherwise you risk Kernel panic, which results in crashing OS.

---

Not working games with this plugin (You can find games not compatible with SaltyNX [here](https://github.com/masagrator/SaltyNX/blob/master/README.md))
| Title | Version(s) | Why? |
| ------------- | ------------- | ------------- |
| The Talos Principle | all | Different graphics API, for FPS counter check [here](https://gbatemp.net/threads/the-talos-principle-graphics-settings.555045/) |
| The Unholy Society | all | Different graphics API (EGL) |

# Troubleshooting
Q: Why game constantly shows 255?

A: This may be a sign that game is using different API. Make an issue and state name of game. Next updates will include support for other graphics APIs.
