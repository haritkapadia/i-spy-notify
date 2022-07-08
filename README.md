# I Spy Notify

I Spy Notify lets you watch Linux desktop notifications and run scripts for each notification. It's great for logging, displaying popups, and playing sounds.

![Quick demo video](./doc/i-spy-notify-video.mp4)

## Installation

Ensure that the following dependencies are installed.
- `gtk+-3.0`
- `json-c`
- `dbus-1`

Download the repository and run the `install` target in the Makefile.

```
git clone 'https://github.com/haritkapadia/i-spy-notify.git'
cd i-spy-notify
make install
```

## Setup

Copy the sample configuration file from `i-spy-notify/doc/examples/simple.json` to `~/.config/i-spy-notify/i-spy-notify.json`.

## Usage

Simply run the `i-spy-notify` program to start. `i-spy-notify` also comes with an XDG Desktop file, which can be copied to your `~/.config/autostart` folder, and a System-D service file, which can be enabled with `systemctl enable --user i-spy-notify.service` and started with `systemctl start --user i-spy-notify.service`.
