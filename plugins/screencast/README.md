# Screen sharing plugin

Shares screens between a computer and a phone in both directions, shown in a small
resizable window. Frames are JPEG images carried as the payload of a normal KDE Connect
packet, so there is no new transport, port or daemon.

## Packets

All three packet types are listed in both `X-KdeConnect-SupportedPacketType` and
`X-KdeConnect-OutgoingPacketType`, because both ends can send and receive each of them.

### `kdeconnect.screencast.request`

Body: empty.

Asks the peer to start sending its screen. Receiving it must not start capturing
automatically: the desktop only opens its window, and the phone opens its activity, which
asks for the system capture permission.

### `kdeconnect.screencast.frame`

Body: empty. The payload is the JPEG image, and the receiver gets the size from the image itself,
so no field has to be kept in sync with the pixels.

### `kdeconnect.screencast.stop`

Body: empty. Stops the stream: the sender stops capturing, and the receiver clears its window
back to the waiting state instead of showing a frozen frame. The phone sends this when the call
the stream was started for ends.

## Behaviour

- Desktop sends at most 5 frames per second, scaled to fit 1280x720 and JPEG encoded with
  quality 60, which keeps a stream well below a megabit on a local network.
- Capture uses `QScreen::grabWindow()` on X11, and on Wayland KWin's `ScreenShot2` interface:
  the caller creates a pipe, passes the write end as the `h` argument of `CaptureWorkspace()`
  and reads the raw image described by the returned `results` map (`type`, `width`, `height`,
  `stride`, `format` as `QImage::Format`, `scale`). KWin answers with a delayed reply, so the
  helper reads the pipe with a timeout and never blocks indefinitely.
- The paired phone notification gets a "Show phone screen" action that opens the window and
  requests the stream, using the plugin's D-Bus interface
  `org.kde.kdeconnect.device.screencast` at `/modules/kdeconnect/devices/<deviceId>/screencast`.

## Known limitations

- Capturing is Linux only: X11 through Qt, Wayland through KWin. Receiving frames works on
  every platform. A failed capture stops the stream and logs a warning.
- Video only, no audio.
- No congestion control: a slower link drops frames at the sending side's fixed rate rather
  than adapting.
- A phone that has not got the matching plugin build will not show the plugin at all, because
  KDE Connect only loads plugins whose packet types are in the peer's capability list.
