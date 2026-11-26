# Filesystem browser plugin

Lets a paired phone list and download files from this computer, using the connection that is
already there. There is no server, port or mount: the phone asks for a directory, gets a
listing, and asks for individual files.

## Packets

### `kdeconnect.filebrowser.request` (received)

Body: `path` (string, absolute path on this computer). An empty or missing path means the home
directory.

Answered with `kdeconnect.filebrowser.listing`.

### `kdeconnect.filebrowser.listing` (sent)

| Key       | Type   | Meaning                                                     |
|-----------|--------|-------------------------------------------------------------|
| `path`    | string | The directory that was listed, canonicalized                  |
| `parent`  | string | Where to go up to, never above the exposed root               |
| `entries` | array  | Objects with `name`, `dir` (bool), `size` (bytes), `modified` (Unix seconds, 0 for folders) |
| `error`   | string | Only present when the directory could not be listed           |

Entries are sorted folders first, then by name. Hidden files are not listed.

### `kdeconnect.filebrowser.download` (received)

Body: `path` (string, absolute). The file is sent back as a standard
`kdeconnect.share.request` with a payload, so the receiving side saves it with the file
transfer code it already has and shows its usual notification.

## What is exposed

- The session user's home directory, resolved once at startup.
- `realpath` is applied to everything, so `..` and symlinks cannot escape the home directory;
  a path outside it is answered with `error`.
- Downloads are limited to regular, readable files. Folders and unreadable files are refused.
- Everything is read-only: nothing can be uploaded, deleted or modified through this plugin.
