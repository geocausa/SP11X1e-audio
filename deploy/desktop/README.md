# SP11 desktop audio policy

## Windows media-key volume step

Native Windows 11 on the SP11 changes the endpoint scalar by **2 percentage
points** for one hardware Volume-Up/Down key press (for example 12% -> 14% ->
12%). The Ubuntu GNOME default observed on the same machine was `volume-step=6`.

That 3x larger Linux key jump made the otherwise tiny endpoint-gain transition
more visible when the keys were abused at high rate. A matched SP7-external-mic
997-Hz stress test at 12% <-> 14% after changing GNOME to 2% tracked the native
Windows physical transition closely.

Apply once per desktop user:

```bash
./deploy/desktop/apply-windows-media-key-volume-step.sh
```

This intentionally does **not** disable `org.gnome.desktop.sound event-sounds`
or `input-feedback-sounds`. A real media-key trace proved GNOME already matches
the useful Windows policy: `audio-volume-change` feedback is audible at idle but
is suppressed while a media stream is continuously RUNNING.
