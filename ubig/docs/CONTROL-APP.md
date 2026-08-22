# UbiG Equalizer control package

`ubig-control` is a small userspace package for the already-running UbiG native
speaker engine. It does not install a kernel, topology, private owner pack or
PipeWire graph and it never restarts audio by itself.

The GTK4 application exposes:

- Dynamic, Movie, Music, Game, Voice, Course and Custom profiles;
- the recovered 20-band SP11 GEQ grid;
- a friendly `-12..+12 dB` scale backed by the engine's exact `-192..+192`
  sixteenth-dB control domain;
- in-place request/ack status without reconstructing the DSP graph;
- per-user profile and Custom-EQ persistence at
  `$XDG_CONFIG_HOME/ubig/control.json` (normally
  `~/.config/ubig/control.json`).

Build the Debian package:

```sh
packaging/debian/build-control-deb.sh
```

The resulting `dist/ubig-control_<version>_<arch>.deb` contains only the GTK
controller, its dependency-free control-page backend, the native `ubigctl`
fallback and desktop metadata. Launch it from the application menu as **UbiG
Equalizer**, or run `ubig-geq`.

`ubig-geq --restore` reapplies the saved profile/curve without opening a
window. Ordinary controls target `$XDG_RUNTIME_DIR/ubig-control-v2`, or the
explicit `UBIG_CONTROL_PATH` override used by tests/labs.

The package is an engine-control deliverable, not a Golden promotion claim.
Long-run and acoustic gates remain separate evidence for replacing the rollback
bridge permanently.
