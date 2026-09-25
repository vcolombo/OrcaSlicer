# OrcaSlicer

An open-source 3D-printing slicer. This fork is developing a headless, container-based web slicer with its own browser UI. The terms below describe that target domain.

## Language

### Web slicer

**Web slicer**:
Headless OrcaSlicer running in a container, together with the browser UI it serves.
_Avoid_: web VNC, remote desktop, web GUI

**Project**:
The user's work in progress: plates, the objects on them and the presets chosen for them. The server holds it and persists it.
_Avoid_: session, workspace, document

**Plate**:
One build plate within a project, with the objects placed on it.
_Avoid_: bed (the physical surface), build area

**Data volume**:
The container's persistent storage for configuration, user presets, projects and artifacts.
_Avoid_: config dir, appdata

### Slicing and printing

**Slice job**:
A request to turn one plate and its presets into G-code. It has its own lifecycle and yields an artifact.
_Avoid_: print job, slicing task

**Artifact**:
The G-code file a slice job produces. It is stored and handled like any other file.
_Avoid_: output, result

**Print job**:
A print running on a printer, started from an artifact.
_Avoid_: slice job, print

**Printer**:
The physical, connected machine that runs print jobs.
_Avoid_: device, printer preset

### Settings

**Preset**:
A named, saved set of print, filament or printer settings.
_Avoid_: profile (for a single preset), config

**System profile**:
A vendor-supplied bundle of presets shipped with the application.
_Avoid_: default preset, built-in preset

### Bambu

**Network plugin**:
Bambu's proprietary library that talks to Bambu printers and Bambu's cloud on the application's behalf. It is downloaded separately and never shipped with the application.
_Avoid_: plugin (on its own; OrcaSlicer has its own plugin system), agent, SDK

### Access

**Setup code**:
A one-time code printed in the container log that authorizes choosing, or resetting, the login password.
_Avoid_: PIN, invite code

**API token**:
A revocable, named credential for scripts and integrations. Browsers use a login session instead.
_Avoid_: API key, password
