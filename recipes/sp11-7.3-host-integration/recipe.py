#!/usr/bin/env python3
"""Export, verify and install the pinned SP11 7.3 binary release; never compile."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import pwd
import re
import shutil
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def relative(value):
    path = PurePosixPath(value)
    require(value and not path.is_absolute() and '..' not in path.parts,
            'invalid relative path: '+value)
    return Path(value)


def output(root, value):
    path = root / relative(value)
    require(path.resolve().is_relative_to(root.resolve()), 'destination escapes root')
    return path


def command(args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs)


def device_id():
    # Hash stays in the private bundle; no unit identity is committed in Git.
    path = Path('/sys/class/dmi/id/product_uuid')
    require(path.is_file(), 'DMI product UUID unavailable')
    return hashlib.sha256(path.read_bytes().strip().lower()).hexdigest()


class Recipe:
    def __init__(self, directory=HERE):
        self.directory = Path(directory)
        self.lock_path = self.directory/'release-lock.json'
        self.lock = json.loads(self.lock_path.read_text())
        self.release = self.lock['release']
        map_path = self.directory/'module-files.sha256'
        require(sha(map_path) == self.lock['module_map_sha256'], 'module index changed')
        self.modules = []
        for line in map_path.read_text().splitlines():
            digest, name = line.split(None, 1)
            relative(name)
            self.modules.append((name, digest))
        require(len(self.modules) == self.lock['module_count'], 'module count differs')
        self.entries = self.lock['files'] + [
            {'key': 'modules/'+name, 'sha256': digest, 'scope': 'module',
             'target': name, 'mode': 0o644} for name, digest in self.modules]
        self.entries += [
            {'key': 'module-metadata/'+name, 'sha256': digest, 'scope': 'module',
             'target': name, 'mode': 0o644} for name, digest in self.lock['module_metadata'].items()]
        keys = [item['key'] for item in self.entries]
        require(len(set(keys)) == len(keys), 'duplicate payload key')
        for item in self.entries:
            relative(item['key'])
            relative(item['target'])
            if item.get('destination_symlink'):
                relative(item['destination_symlink'])
                require('/' not in item['destination_symlink'], 'firmware alias must stay in its directory')
            if item.get('public_source'):
                source = output(self.directory, item['public_source'])
                require(source.is_file() and sha(source) == item['sha256'], 'public host source changed: '+item['public_source'])

        targets = {item['target']: item for item in self.entries if item['scope'] == 'system'}
        for item in self.entries:
            if item.get('destination_symlink'):
                require(item['scope'] == 'system' and item['target'].startswith('lib/firmware/'), 'only firmware aliases are supported')
                canonical = str(Path(item['target']).parent/item['destination_symlink'])
                require(canonical in targets and targets[canonical]['sha256'] == item['sha256'], 'firmware alias requires its identical canonical payload')

    def verify(self, bundle):
        bundle = Path(bundle)
        require(not (bundle/'EXPORT-FAILED').exists(), 'incomplete export')
        metadata = json.loads((bundle/'bundle.json').read_text())
        require(metadata['recipe_id'] == self.lock['recipe_id'], 'wrong recipe')
        require(metadata['lock_sha256'] == sha(self.lock_path), 'different release lock')
        require(metadata['release'] == self.release, 'unqualified kernel release')
        require(re.fullmatch(r'(?:0x[0-9a-fA-F]{2},){5}0x[0-9a-fA-F]{2}',
                             metadata['touch_identity']) is not None, 'invalid private touch identity')
        require(re.fullmatch(r'[0-9a-f]{64}', metadata['device_id_sha256']) is not None,
                'invalid private hardware binding')
        for item in self.entries:
            path = output(bundle/'payload', item['key'])
            require(path.is_file() and not path.is_symlink(), 'missing payload: '+item['key'])
            require(sha(path) == item['sha256'], 'payload mismatch: '+item['key'])
        for name in ['kernel-source.tar.zst', 'kernel-integration.mbox']:
            path = bundle/name
            require(path.is_file() and sha(path) == self.lock['source_artifacts'][name],
                    'source archive/patch series missing or changed: '+name)
        return metadata

    def export(self, destination, home, pack, source_archive, patches):
        destination, home, pack, source_archive, patches = map(Path, [destination, home, pack, source_archive, patches])
        require(not destination.exists(), 'export destination already exists')
        repository = self.directory.parents[1]
        if (repository/'.git').exists():
            require(not destination.resolve().is_relative_to(repository.resolve()),
                    'private bundles must be exported outside the Git checkout')
        require(platform.machine() == 'aarch64' and os.uname().release == self.release,
                'export only from the accepted ARM64 kernel')
        require(Path('/sys/module/gpi/srcversion').read_text().strip() == self.lock['gpi_srcversion'],
                'running GPI differs')
        for name, expected in self.lock['live_module_versions'].items():
            version = Path('/sys/module')/name/'srcversion'
            require(version.is_file() and version.read_text().strip() == expected,
                    'running module differs: '+name)
        arguments = Path('/proc/cmdline').read_text().split()
        private_key = self.lock['private_kernel_arg']+'='
        identity = [arg[len(private_key):] for arg in arguments if arg.startswith(private_key)]
        require(len(identity) == 1, 'private touch argument missing or duplicated')
        require(source_archive.is_file() and patches.is_file(), 'provide the complete source archive and patch series')
        destination.mkdir(parents=True, mode=0o700)
        modules = Path('/usr/lib/modules')/self.release
        try:
            for index, item in enumerate(self.entries):
                scope = item.get('source_scope')
                if scope == 'system':
                    source = Path('/')/item['source']
                elif scope == 'user':
                    source = home/item['source']
                elif scope == 'recipe':
                    source = self.directory/item['source']
                elif scope == 'private_pack':
                    source = pack
                else:
                    source = modules/item['target']
                require(source.is_file() and sha(source) == item['sha256'], 'export source differs: '+str(source))
                target = output(destination/'payload', item['key'])
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
                if not target.is_symlink(): target.chmod(item['mode'])
                if index and index % 2000 == 0:
                    print('Copied', index, 'verified files', flush=True)
            shutil.copyfile(source_archive, destination/'kernel-source.tar.zst')
            shutil.copyfile(patches, destination/'kernel-integration.mbox')
            metadata = {'recipe_id': self.lock['recipe_id'], 'release': self.release,
                        'lock_sha256': sha(self.lock_path), 'touch_identity': identity[0],
                        'device_id_sha256': device_id()}
            (destination/'bundle.json').write_text(json.dumps(metadata, indent=2)+'\n')
            self.verify(destination)
        except BaseException:
            # Keep failed exports for inspection; never treat them as release-ready.
            (destination/'EXPORT-FAILED').touch()
            raise
        owner = home.stat()
        for path in [destination, *destination.rglob('*')]:
            os.chown(path, owner.st_uid, owner.st_gid)
        return metadata

    def plan(self, bundle, home, uid, root_uuid):
        metadata = self.verify(bundle)
        require(re.fullmatch(r'/[A-Za-z0-9_./-]+', home) and '..' not in Path(home).parts,
                'use a standard absolute Linux home path without whitespace')
        require(uid > 0 and re.fullmatch(r'[0-9a-fA-F-]{36}', root_uuid), 'invalid UID/root UUID')
        planned = []
        for item in self.entries:
            scope = item['scope']
            if scope == 'user':
                target = home.lstrip('/')+'/'+item['target']
            elif scope == 'module':
                target = 'usr/lib/modules/'+self.release+'/'+item['target']
            elif scope == 'boot':
                target = 'boot/sp11-baseline73/'+item['target']
            else:
                target = item['target']
            planned.append({**item, 'destination': target})
        args = [*self.lock['kernel_args'], 'root=UUID='+root_uuid, 'sp11_entry=baseline73',
                self.lock['private_kernel_arg']+'='+metadata['touch_identity']]
        recipe = ('#!/bin/sh\nexec tail -n +3 "$0"\n'
                  "menuentry 'Ubuntu SP11 7.3 sound/touch/Wi-Fi baseline' --id 'sp11-baseline73' {\n"
                  ' load_video\n set gfxpayload=keep\n insmod gzio\n insmod part_gpt\n insmod ext2\n insmod fdt\n'
                  ' search --no-floppy --fs-uuid --set=root '+root_uuid+'\n'
                  ' devicetree /boot/sp11-baseline73/x1e80100-microsoft-denali-oled-final.dtb\n'
                  ' linux /boot/sp11-baseline73/vmlinuz-'+self.release+' '+' '.join(args)+'\n'
                  ' initrd /boot/sp11-baseline73/initrd.img-'+self.release+'\n}\n')
        return planned, recipe

    def populate(self, bundle, root, home, uid, root_uuid, *, live=False):
        root = Path(root).resolve()
        if not live:
            require(root != Path('/') and (not root.exists() or not any(root.iterdir())),
                    'stage root must be empty and must not be /')
        planned, recipe = self.plan(bundle, home, uid, root_uuid)
        self.created_dirs = []
        def parent(directory):
            missing = []
            while not directory.exists():
                missing.append(directory)
                directory = directory.parent
            for path in reversed(missing):
                path.mkdir()
                self.created_dirs.append(path)
        root.mkdir(parents=True, exist_ok=True)
        if not live:
            # Ubuntu uses merged-/usr. This also makes offline depmod find /lib/modules.
            (root/'usr/lib').mkdir(parents=True, exist_ok=True)
            (root/'lib').symlink_to('usr/lib')
        for item in planned:
            source = output(Path(bundle)/'payload', item['key'])
            target = output(root, item['destination'])
            parent(target.parent)
            if item.get('destination_symlink'):
                target.unlink(missing_ok=True)
                target.symlink_to(item['destination_symlink'])
            elif item.get('template'):
                target.write_text(source.read_text().replace('@HOME@', home).replace('@UID@', str(uid)))
            else:
                shutil.copyfile(source, target)
            if not target.is_symlink(): target.chmod(item['mode'])
        target = output(root, 'etc/grub.d/79_sp11_baseline73')
        parent(target.parent)
        target.write_text(recipe)
        target.chmod(0o755)
        # Persistent user enablement without invoking/restarting the live user graph.
        wants = output(root, home.lstrip('/')+'/.config/systemd/user/default.target.wants')
        parent(wants)
        for name in ['sp11-ubig-volume-sync.service', 'sp11-ubig-monitor-link.service']:
            (wants/name).symlink_to('../'+name)
        (wants/'filter-chain.service').symlink_to('/usr/lib/systemd/user/filter-chain.service')
        manifest = {'recipe_id': self.lock['recipe_id'], 'release': self.release,
                    'home': home, 'uid': uid, 'root_uuid': root_uuid,
                    'files': [{'destination': item['destination'],
                               'sha256': sha(output(root, item['destination']))} for item in planned]}
        state = output(root, 'var/lib/sp11-baseline73')
        state.mkdir(parents=True, exist_ok=True)
        (state/'installed.json').write_text(json.dumps(manifest, indent=2)+'\n')
        return manifest

    def refresh_generated_metadata(self, manifest, root):
        prefix = 'usr/lib/modules/'+self.release+'/modules.'
        for row in manifest['files']:
            if row['destination'].startswith(prefix):
                row['sha256'] = sha(Path(root)/row['destination'])

    def install(self, bundle, username, unsigned_boot):
        require(os.geteuid() == 0, 'installation requires root')
        require(unsigned_boot, 'confirm firmware can boot this unsigned kernel with --unsigned-boot-ready')
        for tool in ['depmod', 'dracut', 'lsinitrd', 'update-grub', 'grub-script-check', 'grub-editenv', 'findmnt']:
            require(shutil.which(tool) is not None, 'missing installation tool: '+tool)
        metadata = self.verify(bundle)
        require(device_id() == metadata['device_id_sha256'], 'bundle belongs to a different hardware unit')
        os_release = dict(line.split('=', 1) for line in Path('/etc/os-release').read_text().splitlines() if '=' in line)
        for key in ['ID', 'VERSION_ID']:
            require(os_release[key].strip('"') == self.lock['tested_os'][key], 'unqualified OS '+key)
        require(platform.machine() == self.lock['tested_os']['architecture'], 'unqualified architecture')
        for name, version in self.lock['tested_packages'].items():
            actual = command(['dpkg-query', '-W', '-f=${Version}', name]).strip()
            require(actual == version, 'unqualified package '+name+': '+actual)
        person = pwd.getpwnam(username)
        require(person.pw_uid > 0, 'select the desktop user, not root')
        home = Path(person.pw_dir)
        require(home.is_dir() and home.stat().st_uid == person.pw_uid, 'invalid desktop home')
        require(not (Path('/usr/lib/modules')/self.release).exists(),
                'release already installed; refuse to overwrite a running or existing baseline')
        require(not Path('/boot/sp11-baseline73').exists() and not Path('/etc/grub.d/79_sp11_baseline73').exists(),
                'recipe boot assets already exist')
        require(not Path('/var/lib/sp11-baseline73').exists(), 'previous recipe state exists')
        for masked in [home/'.config/systemd/user/filter-chain.service',
                       Path('/etc/systemd/user/filter-chain.service')]:
            require(not masked.is_symlink() or os.readlink(masked) != '/dev/null',
                    'filter-chain service is masked; resolve that before installing')
        wants = home/'.config/systemd/user/default.target.wants'
        for name in ['filter-chain.service', 'sp11-ubig-volume-sync.service', 'sp11-ubig-monitor-link.service']:
            require(not (wants/name).exists() and not (wants/name).is_symlink(),
                    'existing user enablement: '+name)
        for name in ['filter-chain.service.d', '../../pipewire/filter-chain.conf.d', '../../wireplumber/wireplumber.conf.d']:
            directory = home/'.config/systemd/user'/name
            require(not directory.exists() or not any(directory.iterdir()), 'existing audio configuration: '+str(directory))
        environment = command(['grub-editenv', '/boot/grub/grubenv', 'list'])
        require(all(not line.startswith('next_entry=') or line == 'next_entry=' for line in environment.splitlines()),
                'another next boot is already selected')
        root_uuid = command(['findmnt', '-n', '-o', 'UUID', '-T', '/']).strip()
        require(command(['findmnt', '-n', '-o', 'FSTYPE', '-T', '/']).strip() == 'ext4',
                'only the tested ext4 layout is qualified')
        require(command(['findmnt', '-n', '-o', 'UUID', '-T', '/boot']).strip() == root_uuid,
                'separate /boot filesystem requires a separately qualified recipe')
        planned, _ = self.plan(bundle, str(home), person.pw_uid, root_uuid)
        for item in planned:
            if item['scope'] == 'user':
                target = Path('/')/item['destination']
                require(not target.exists() and not target.is_symlink(),
                        'fresh-user recipe refuses existing SP11 file: '+str(target))
        required = sum(output(Path(bundle)/'payload', item['key']).stat().st_size for item in planned)
        require(shutil.disk_usage('/').free > required + 2*1024**3, 'insufficient installation scratch space')
        backup = Path('/var/backups/sp11-baseline73')
        require(not backup.exists(), 'backup directory exists; inspect it before retrying')
        backup.mkdir(mode=0o700)
        previous = []
        self.previous_symlinks = {}
        for item in planned:
            target = Path('/')/item['destination']
            if target.is_symlink():
                require(item.get('destination_symlink') == os.readlink(target), 'unexpected existing symlink at '+str(target))
                self.previous_symlinks[item['destination']] = os.readlink(target)
                continue
            if target.is_file():
                saved = backup/item['destination']
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, saved)
                previous.append(item['destination'])
        shutil.copy2('/boot/grub/grub.cfg', backup/'grub.cfg')
        (backup/'restore-symlinks.json').write_text(json.dumps(self.previous_symlinks, indent=2)+'\n')
        (backup/'restore-files.json').write_text(json.dumps(previous, indent=2)+'\n')
        (backup/'planned-files.json').write_text(json.dumps(planned, indent=2)+'\n')
        from host_policy import HostPolicy
        host = HostPolicy(self, bundle, backup) if self.lock.get('host_policy') else None
        try:
            manifest = self.populate(bundle, Path('/'), str(home), person.pw_uid, root_uuid, live=True)
            # Set ownership only on recipe-owned user paths and directories.
            for item in planned:
                if item['scope'] == 'user':
                    target = Path('/')/item['destination']
                    os.chown(target, person.pw_uid, person.pw_gid)
            for directory in self.created_dirs:
                if directory.is_relative_to(home):
                    os.chown(directory, person.pw_uid, person.pw_gid)
            wants = home/'.config/systemd/user/default.target.wants'
            for link in wants.iterdir():
                if link.name in ['filter-chain.service', 'sp11-ubig-volume-sync.service', 'sp11-ubig-monitor-link.service']:
                    os.lchown(link, person.pw_uid, person.pw_gid)
            if host: host.apply()
            command(['depmod', self.release])
            image = '/boot/sp11-baseline73/initrd.img-'+self.release
            firmware = ['/'+item['target'] for item in planned
                        if item['scope'] == 'system' and item['target'].startswith('lib/firmware/')]
            command(['dracut', '--force', '--hostonly', '--no-hostonly-cmdline', '--nostrip',
                     '--add', 'initqueue sp11-kmod', '--remove', '/etc/udev/rules.d/99-sp11-battery-reporting.rules',
                     '--add-drivers', self.lock['force_drivers'], '--install', ' '.join(firmware), image, self.release])
            self.check_initrd(Path(image))
            self.refresh_generated_metadata(manifest, Path('/'))
            body = command(['sh', '/etc/grub.d/79_sp11_baseline73'])
            subprocess.run(['grub-script-check'], input=body, text=True, check=True)
            command(['update-grub'])
            command(['grub-script-check', '/boot/grub/grub.cfg'])
            require(command(['grub-editenv', '/boot/grub/grubenv', 'list']) == environment, 'boot selection changed')
            # Keep the existing default; require an explicit one-shot command after review.
            manifest['initrd_verified'] = True
            manifest['previous_default_preserved'] = True
            Path('/var/lib/sp11-baseline73/installed.json').write_text(json.dumps(manifest, indent=2)+'\n')
        except BaseException:
            if host: host.rollback_begin()
            try:
                self.rollback(person, planned, previous, backup)
            finally:
                if host: host.rollback_finish()
            raise
        return manifest

    def check_live(self):
        require(os.geteuid() == 0, 'run live verification as root to read protected firmware')
        state = json.loads(Path('/var/lib/sp11-baseline73/installed.json').read_text())
        require(os.uname().release == self.release and state['release'] == self.release,
                'wrong kernel booted')
        args = Path('/proc/cmdline').read_text().split()
        require('sp11_entry=baseline73' in args, 'wrong entry booted')
        require([arg for arg in args if arg.startswith('root=')] == ['root=UUID='+state['root_uuid']],
                'different root filesystem selected')
        require(not any(arg.startswith(('trace_event=', 'ftrace=')) for arg in args), 'diagnostic tracing requested')
        require(Path('/sys/module/gpi/srcversion').read_text().strip() == self.lock['gpi_srcversion'],
                'different GPI loaded')
        for name, version in self.lock['live_module_versions'].items():
            require((Path('/sys/module')/name/'srcversion').read_text().strip() == version,
                    'different loaded module: '+name)
        for item in state['files']:
            require(sha(Path('/')/item['destination']) == item['sha256'], 'installed file differs: '+item['destination'])
        stats = Path('/sys/bus/spi/devices/spi0.0/behavior_stats').read_text()
        touch = dict(line.split('=', 1) for line in stats.splitlines() if '=' in line)
        require(touch.get('mode_enabled') == '1', 'touch initialization failed')
        require(int(touch.get('output_contacts', '0')) > 0, 'touch the screen, then repeat verification')
        for key in ['heat_errors', 'irq_transport_errors', 'irq_protocol_errors', 'recovery_failures']:
            require(touch.get(key) == '0', 'touch errors: '+key)
        for name in ['sp11_feedback_active_offset2_zero', 'sp11_cps_pcm_route_105c']:
            require((Path('/sys/module/soundwire_qcom/parameters')/name).read_text().strip() == 'Y', name)
        for name in ['WSA WSA_COMP1 Switch', 'WSA WSA_COMP2 Switch']:
            require('values=on' in command(['amixer', '-D', 'hw:0', 'cget', 'name='+name]), 'compander off: '+name)
        log = command(['journalctl', '-b', '-k', '--no-pager'])
        for marker in ['Oops:', 'BUG:', 'WARNING:', 'Unhandled context fault:',
                       'SPI TX transfer timed out', 'SPI RX transfer timed out',
                       'Initialization not complete']:
            require(marker not in log, 'kernel fault: '+marker)
        person = pwd.getpwuid(state['uid'])
        require(person.pw_dir == state['home'], 'desktop identity differs from installed manifest')
        user = ['runuser', '-u', person.pw_name, '--', 'env',
                'XDG_RUNTIME_DIR=/run/user/'+str(person.pw_uid),
                'DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/'+str(person.pw_uid)+'/bus']
        nodes = json.loads(command(user+['pw-dump'], timeout=15))
        props = [item.get('info', {}).get('props', {}) for item in nodes if item.get('type') == 'PipeWire:Interface:Node']
        require(any(row.get('node.name') == 'effect_input.sp11_ubig' for row in props), 'processed output missing')
        require(any(row.get('node.name') == 'alsa_output.platform-sound.HiFi__Speaker__sink'
                    and row.get('node.hidden') in [True, 'true'] for row in props), 'physical backend is not hidden')
        default = command(user+['wpctl', 'inspect', '@DEFAULT_AUDIO_SINK@'], timeout=15)
        require('node.name = "effect_input.sp11_ubig"' in default, 'select the visible UBiG sink as default')
        for unit in ['filter-chain.service', 'sp11-ubig-volume-sync.service', 'sp11-ubig-monitor-link.service']:
            require(command(user+['systemctl', '--user', 'is-active', unit], timeout=15).strip() == 'active', unit)
        dispatcher = command(user+['journalctl', '--user', '-b', '-u', 'sp11-ubig-volume-sync.service',
                                   '--no-pager'], timeout=15)
        require('selected=windows-transaction' in dispatcher, 'transaction volume actuator was not selected')
        require('MUTED' in command(user+['wpctl', 'get-volume', '@DEFAULT_AUDIO_SOURCE@'], timeout=15),
                'keep the unqualified microphone muted')
        if self.lock.get('host_policy'):
            from host_policy import verify_live
            verify_live(self.lock['host_policy'])
        return {'recipe_id': self.lock['recipe_id'], 'files_verified': len(state['files']),
                'touch_contacts': int(touch['output_contacts']), 'kernel_fault_signatures': 0,
                'physical_audio_confirmation': 'still required: both speakers, two cold starts',
                'default_change': 'not performed by verifier'}

    def check_initrd(self, image):
        with tempfile.TemporaryDirectory(prefix='sp11-initrd-check-') as name:
            root = Path(name)
            command(['lsinitrd', '--unpackearly', str(image.resolve())], cwd=root)
            command(['lsinitrd', '--unpack', str(image.resolve())], cwd=root)
            prefix = root/'usr/lib/modules'/self.release
            found = {str(path.relative_to(prefix)): sha(path) for path in prefix.rglob('*.ko')}
            require(found, 'initrd has no matching modules')
            expected = dict(self.modules)
            for name, digest in found.items():
                require(expected.get(name) == digest, 'initrd module differs: '+name)
            for required in ['ath12k.ko', 'ath12k_wifi7.ko', 'gpi.ko', 'spi-geni-qcom.ko', 'mshw0485_touch.ko',
                             'snd-soc-lpass-wsa-macro.ko', 'snd-soc-wsa884x.ko', 'snd-q6apm.ko', 'soundwire-qcom.ko']:
                require(any(Path(name).name == required for name in found), 'initrd lacks '+required)
            require(set(self.lock['live_module_versions']) <= {Path(name).stem.replace('-', '_') for name in found}, 'critical initrd driver missing')
            require((root/'usr/sbin/modprobe').resolve() == root/'usr/bin/kmod', 'initrd modprobe must use kmod')
            loader = command(['chroot', str(root), '/sbin/modprobe', '--ignore-install', '--dry-run', '--verbose', 'snd'])
            require('insmod ' in loader, 'real kmod dry-run failed')
            require((root/'usr/sbin/initqueue').exists() or (root/'sbin/initqueue').exists(), 'initqueue missing')
            require(not (root/'etc/udev/rules.d/99-sp11-battery-reporting.rules').exists(), 'root-only battery guard included in initrd')
            early_load = root/'usr/bin/dracut-pre-udev'
            require(early_load.is_file() and 'getargs rd.driver.pre' in early_load.read_text(),
                    'early Wi-Fi preload handler missing')
            for item in self.lock['files']:
                if item['scope'] == 'system' and item['target'].startswith('lib/firmware/'):
                    require(sha(root/item['target']) == item['sha256'], 'initrd firmware differs')
            # Fresh UUID comes from the new GRUB entry; no saved old host cmdline.
            for path in (root/'etc/cmdline.d').glob('*'):
                require('root=UUID=' not in path.read_text(errors='replace'), 'host UUID embedded in initrd')
            return len(found)

    def rollback(self, person, planned, previous, backup):
        for item in planned:
            target = Path('/')/item['destination']
            if target.is_symlink(): target.unlink()
            if item['destination'] in previous:
                shutil.copy2(backup/item['destination'], target)
            else:
                target.unlink(missing_ok=True)
        for name, value in getattr(self, 'previous_symlinks', {}).items():
            target = Path('/')/name
            target.unlink(missing_ok=True)
            target.symlink_to(value)
        for directory in [Path('/boot/sp11-baseline73'), Path('/usr/lib/modules')/self.release,
                          Path('/var/lib/sp11-baseline73')]:
            shutil.rmtree(directory, ignore_errors=True)
        Path('/etc/grub.d/79_sp11_baseline73').unlink(missing_ok=True)
        for name in ['filter-chain.service', 'sp11-ubig-volume-sync.service', 'sp11-ubig-monitor-link.service']:
            (Path(person.pw_dir)/'.config/systemd/user/default.target.wants'/name).unlink(missing_ok=True)
        for directory in reversed(getattr(self, 'created_dirs', [])):
            if directory.is_relative_to(Path(person.pw_dir)):
                try:
                    directory.rmdir()
                except OSError:
                    pass
        shutil.copy2(backup/'grub.cfg', '/boot/grub/grub.cfg')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='action', required=True)
    export = commands.add_parser('export', help='copy the accepted machine into a PRIVATE bundle; no build')
    export.add_argument('--home', required=True, type=Path)
    export.add_argument('--private-pack', required=True, type=Path)
    export.add_argument('--kernel-source', required=True, type=Path)
    export.add_argument('--kernel-patches', required=True, type=Path)
    export.add_argument('--output', required=True, type=Path)
    for action in ['verify', 'stage', 'install', 'check-initrd']:
        sub = commands.add_parser(action)
        if action == 'check-initrd':
            sub.add_argument('image', type=Path)
        else:
            sub.add_argument('bundle', type=Path)
        if action == 'stage':
            sub.add_argument('--root', required=True, type=Path)
            sub.add_argument('--home', required=True)
            sub.add_argument('--uid', required=True, type=int)
            sub.add_argument('--root-uuid', required=True)
        if action == 'install':
            sub.add_argument('--user', required=True)
            sub.add_argument('--unsigned-boot-ready', action='store_true')
    commands.add_parser('check-live', help='read-only postboot checks; never play or record audio')
    args = parser.parse_args()
    recipe = Recipe()
    if args.action == 'export':
        result = recipe.export(args.output, args.home, args.private_pack, args.kernel_source, args.kernel_patches)
        result = {'recipe_id': result['recipe_id'], 'files_verified': len(recipe.entries)}
    elif args.action == 'verify':
        metadata = recipe.verify(args.bundle)
        result = {'recipe_id': metadata['recipe_id'], 'files_verified': len(recipe.entries)}
    elif args.action == 'stage':
        manifest = recipe.populate(args.bundle, args.root, args.home, args.uid, args.root_uuid)
        result = {'staged_files': len(manifest['files']), 'root': str(args.root), 'host_commands_run': False}
    elif args.action == 'install':
        result = recipe.install(args.bundle, args.user, args.unsigned_boot_ready)
    elif args.action == 'check-initrd':
        result = {'initrd_modules_verified': recipe.check_initrd(args.image)}
    else:
        result = recipe.check_live()
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit('SP11 recipe refused: '+str(error))
