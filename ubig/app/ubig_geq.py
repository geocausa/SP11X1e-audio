#!/usr/bin/python3
"""GTK4 profile and 20-band GEQ controller for the live UbiG engine."""

from __future__ import annotations

import argparse
import sys

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import GLib, Gtk  # noqa: E402

from ubig_control import (
    BAND_FREQUENCIES,
    ControlPage,
    PROFILE_CUSTOM,
    PROFILE_NAMES,
    db_to_raw,
    load_saved_state,
    raw_to_db,
    save_state,
)


def frequency_label(frequency: int) -> str:
    if frequency >= 1000:
        value = frequency / 1000.0
        return f"{value:g}k"
    return str(frequency)


class UbigWindow(Gtk.ApplicationWindow):
    def __init__(self, application: Gtk.Application):
        super().__init__(application=application, title="UbiG Equalizer")
        self.set_default_size(1080, 540)
        self.control = ControlPage(create=True)
        snapshot = self.control.snapshot()
        self.pending_generation: int | None = None

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        outer.set_margin_top(18)
        outer.set_margin_bottom(18)
        outer.set_margin_start(18)
        outer.set_margin_end(18)
        self.set_child(outer)

        title = Gtk.Label(label="UbiG 20-band speaker equalizer")
        title.add_css_class("title-1")
        title.set_xalign(0)
        outer.append(title)

        subtitle = Gtk.Label(
            label="Changes are applied in place to the native userspace engine; the audio graph is not restarted."
        )
        subtitle.set_xalign(0)
        subtitle.set_wrap(True)
        outer.append(subtitle)

        controls = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        outer.append(controls)
        controls.append(Gtk.Label(label="Profile"))
        self.profile = Gtk.DropDown.new_from_strings(PROFILE_NAMES)
        desired = snapshot.desired_profile if snapshot.desired_profile < len(PROFILE_NAMES) else 0
        self.profile.set_selected(desired)
        controls.append(self.profile)

        apply_profile = Gtk.Button(label="Apply profile")
        apply_profile.connect("clicked", self.on_apply_profile)
        controls.append(apply_profile)

        flat = Gtk.Button(label="Flat")
        flat.connect("clicked", self.on_flat)
        controls.append(flat)

        restore = Gtk.Button(label="Restore saved")
        restore.connect("clicked", self.on_restore)
        controls.append(restore)

        apply_eq = Gtk.Button(label="Apply Custom EQ")
        apply_eq.add_css_class("suggested-action")
        apply_eq.connect("clicked", self.on_apply_eq)
        controls.append(apply_eq)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.NEVER)
        scroll.set_vexpand(True)
        outer.append(scroll)
        bands = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=7)
        bands.set_margin_top(8)
        bands.set_margin_bottom(8)
        scroll.set_child(bands)

        self.scales: list[Gtk.Scale] = []
        for frequency, raw in zip(BAND_FREQUENCIES, snapshot.custom_eq):
            column = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
            column.set_size_request(46, 330)
            label = Gtk.Label(label=frequency_label(frequency))
            label.add_css_class("caption")
            column.append(label)
            scale = Gtk.Scale.new_with_range(Gtk.Orientation.VERTICAL, -12.0, 12.0, 0.5)
            scale.set_inverted(True)
            scale.set_digits(1)
            scale.set_draw_value(True)
            scale.set_value_pos(Gtk.PositionType.BOTTOM)
            scale.set_vexpand(True)
            scale.set_value(raw_to_db(raw))
            scale.set_tooltip_text(f"{frequency} Hz, -12 to +12 dB")
            column.append(scale)
            self.scales.append(scale)
            bands.append(column)

        self.status = Gtk.Label()
        self.status.set_xalign(0)
        outer.append(self.status)
        self.update_status(snapshot)
        self.poll_id = GLib.timeout_add(250, self.poll_ack)
        self.connect("close-request", self.on_close)

    def current_eq(self) -> tuple[int, ...]:
        return tuple(db_to_raw(scale.get_value()) for scale in self.scales)

    def update_status(self, snapshot=None) -> None:
        if snapshot is None:
            snapshot = self.control.snapshot()
        active = PROFILE_NAMES[snapshot.active_profile] if snapshot.active_profile < len(PROFILE_NAMES) else "Unknown"
        if snapshot.last_error:
            self.status.set_text(f"Engine rejected the last request: {snapshot.last_error}")
        elif snapshot.request_pending:
            self.status.set_text(
                f"Queued generation {snapshot.request_generation}; engine acknowledged {snapshot.ack_generation}."
            )
        else:
            self.status.set_text(f"Active profile: {active} · generation {snapshot.ack_generation} applied")

    def poll_ack(self) -> bool:
        try:
            snapshot = self.control.snapshot()
            self.update_status(snapshot)
            if self.pending_generation is not None and snapshot.ack_generation == self.pending_generation:
                self.pending_generation = None
        except Exception as error:
            self.status.set_text(f"Control page unavailable: {error}")
        return GLib.SOURCE_CONTINUE

    def on_apply_profile(self, _button: Gtk.Button) -> None:
        selected = int(self.profile.get_selected())
        if selected == PROFILE_CUSTOM:
            self.on_apply_eq(_button)
            return
        self.pending_generation = self.control.request_profile(selected)
        save_state(selected, self.current_eq())
        self.update_status()

    def on_apply_eq(self, _button: Gtk.Button) -> None:
        values = self.current_eq()
        self.pending_generation = self.control.request_custom_eq(values)
        self.profile.set_selected(PROFILE_CUSTOM)
        save_state(PROFILE_CUSTOM, values)
        self.update_status()

    def on_flat(self, button: Gtk.Button) -> None:
        for scale in self.scales:
            scale.set_value(0.0)
        self.on_apply_eq(button)

    def on_restore(self, _button: Gtk.Button) -> None:
        try:
            profile, values = load_saved_state()
            for scale, raw in zip(self.scales, values):
                scale.set_value(raw_to_db(raw))
            self.profile.set_selected(profile)
            if profile == PROFILE_CUSTOM:
                self.pending_generation = self.control.request_custom_eq(values)
            else:
                self.pending_generation = self.control.request_profile(profile)
            self.update_status()
        except Exception as error:
            self.status.set_text(f"Could not restore saved controls: {error}")

    def on_close(self, _window: Gtk.Window) -> bool:
        if self.poll_id:
            GLib.source_remove(self.poll_id)
            self.poll_id = 0
        self.control.close()
        return False


class UbigApplication(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="io.github.geocausa.UbiG")

    def do_activate(self) -> None:
        window = self.props.active_window
        if window is None:
            window = UbigWindow(self)
        window.present()


def restore_saved() -> int:
    profile, values = load_saved_state()
    with ControlPage(create=True) as control:
        if profile == PROFILE_CUSTOM:
            generation = control.request_custom_eq(values)
        else:
            generation = control.request_profile(profile)
    print(f"restored {PROFILE_NAMES[profile]} generation={generation}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Control the live UbiG speaker engine")
    parser.add_argument("--restore", action="store_true", help="restore the saved profile/EQ without opening the GUI")
    args, gtk_args = parser.parse_known_args(argv)
    if args.restore:
        return restore_saved()
    return UbigApplication().run([sys.argv[0], *gtk_args])


if __name__ == "__main__":
    raise SystemExit(main())
