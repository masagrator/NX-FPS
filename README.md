# NX-FPS

SaltyNX plugin that collects FPS data in Nintendo Switch games. You need my fork of SaltyNX installed.
https://github.com/masagrator/SaltyNX/releases

Put NX-FPS.elf to `/SaltySD/plugins`

Currently supported graphics APIs:
- NVN

When game is booted, plugin outputs one file:
```
/SaltySD/FPSoffset.hex
```

There is stored address, where you can find PFPS, FPS has address `PFPS - 0x8`.

>PFPS - Pushed Frames Per Second (u8), it counts how many frames were actually pushed to display in second that passed.
>FPS - Frames Per Second (float) caculated from averaged frametime, refreshed with each new frame.

If file is already there, it's rewritten by new address with each new game boot.

To show it on display, you can use Status Monitor Overlay >=0.4
https://github.com/masagrator/Status-Monitor-Overlay

You can also make your own homebrew to use this plugin.

>WARNING

Plugin brings some instability to boot process for some games. It is recommended to not close game before ~10 seconds have passed from showing Nintendo logo, otherwise you risk Kernel panic, which results in crashing OS.

---

Not working games with this plugin (You can find games not compatible with SaltyNX [here](https://github.com/masagrator/SaltyNX/blob/master/README.md))
| Title | Version(s) | Why? |
| ------------- | ------------- | ------------- |
| LAYTON'S MYSTERY JOURNEY: Katrielle and the Millionaires' Conspiracy | all | Different graphics API (OpenGL/EGL) |
| Mosaic | all | Not using nvnQueuePresentTexture to push frames (reason unknown) |
| The Talos Principle | all | Different graphics API, for FPS counter check [here](https://gbatemp.net/threads/the-talos-principle-graphics-settings.555045/) |
| The Unholy Society | all | Different graphics API (OpenGL/EGL) |

# Troubleshooting
Q: Why I got constantly 255?

A: 255 is default value before plugin starts counting frames. This may be a sign that game is using different API or function than what is currently supported. Make an issue and state name of game. Next updates will include support for other graphics APIs.

# Thanks to:

- RetroNX channel for help with coding stuff,
- CTCaer for providing many useful informations and convincing me to the end that I should target high precision,
- Herbaciarz for providing video footage.
