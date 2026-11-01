#!/bin/bash
# Ubuntu's ALSA install hooks require kmod's --ignore-install option.
# BusyBox installs its modprobe applet before dracut's base module and that
# symlink survives an ordinary inst_multiple modprobe call.
check() { require_binaries kmod || return 1; return 255; }
depends() { echo base; }
install() {
    inst_multiple kmod || return 1
    local target="$initdir/usr/sbin/modprobe"
    if [[ -L $target ]]; then
        rm -- "$target" || return 1
    elif [[ -e $target ]]; then
        dfatal 'Refusing to replace an unexpected real modprobe binary'
        return 1
    fi
    ln_r /usr/bin/kmod /usr/sbin/modprobe
}
