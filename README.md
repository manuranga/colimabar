# ColimaBar

Menu bar status of [colima](https://github.com/abiosoft/colima): running containers, and what the VMs cost the Mac. The menu has a section per running profile, its containers grouped by k3d cluster and compose project, busiest first.

![UI preview](TODO)

"On Mac" is the VM's Virtualization.framework process, so its RAM is what macOS sees, often the VM's whole memory even when containers use little. Container CPU and memory come from the Docker API on `~/.colima/<profile>/docker.sock`, as in `docker stats`.

## Setup

    make cert   # one-time: self-signed identity so privacy grants survive rebuilds
    make        # builds dist/ColimaBar.app and dist/com.manu.colimabar.plist
    cp .config.example .config

Try a build without installing: `open dist/ColimaBar.app --args $PWD/.config`

## Install with Finder

The LaunchAgent runs `installed/ColimaBar.app`, so rebuilding `dist/` never touches it. Install by copying with Finder, not a script: SentinelOne treats scripted changes to a LaunchAgent and its app as `persistence_deception`, kills the terminal's whole process tree and quarantines the files.

1. In `~/proj/colimabar`, create folder `installed` and Option-drag `dist/ColimaBar.app` into it.
2. Cmd-Shift-G to `~/Library/LaunchAgents`, Option-drag `dist/com.manu.colimabar.plist` into it.
3. Start it now (otherwise at next login), in Terminal.app:

       launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.manu.colimabar.plist

To update, never replace the app while the plist points at it:

1. Stop: `launchctl bootout gui/$(id -u)/com.manu.colimabar`
2. Move `~/Library/LaunchAgents/com.manu.colimabar.plist` to the Trash.
3. Replace `installed/ColimaBar.app` with `dist/ColimaBar.app`, then do install steps 2 and 3.

To uninstall: steps 1 and 2 of updating.

Logs: `~/Library/Logs/colimabar.log`

## Config

`.config` (gitignored), `key=value`:

- `status_interval`: seconds between polls
- `busy_cpu`: VM CPU percent on the Mac above which the menu bar shows it

## License

[Apache 2.0](LICENSE)
