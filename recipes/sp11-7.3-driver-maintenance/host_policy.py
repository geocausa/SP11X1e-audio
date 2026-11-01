"""Guarded UPower/host integration for the qualified Ubuntu release.

Used only by explicit fresh installation; imports and offline staging do not
change the host. Charging controls and user security preferences are retained.
"""
from pathlib import Path
import json,os,shutil,subprocess


def command(*args):
    return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT,timeout=90)


def thresholds():
    root=Path('/sys/class/power_supply/qcom-battmgr-bat')
    return {f.name:f.read_text() for f in root.glob('charge_control_*_threshold')}


def unconfigured_sssd_links(domain, fragments, links):
    if domain.exists() or (fragments.exists() and list(fragments.iterdir())):
        return []
    result=[]
    for name,target in links.items():
        path=Path(name)
        if path.exists() or path.is_symlink():
            recipe_require(path.is_symlink() and os.readlink(path)==target,'unexpected SSSD enablement: '+name)
            result.append((name,target))
    return result


class HostPolicy:
    def __init__(self, recipe, bundle, backup):
        self.recipe,self.bundle,self.backup=recipe,Path(bundle),Path(backup)
        self.pin=recipe.lock['host_policy'];self.changed=False;self.removed=[]
        self.before_thresholds=thresholds()
        for name,pin in self.pin['preconditions'].items():
            path=Path(name)
            if pin is None:
                recipe_require(not path.exists() and not path.is_symlink(),'existing local host override: '+name)
            else:
                recipe_require(path.is_file() and not path.is_symlink() and recipe_sha(path)==pin,'host precondition differs: '+name)
        self.stock=self.backup/'stock-upower.deb'
        source=self.bundle/'payload'/self.pin['stock_package_key']
        shutil.copyfile(source,self.stock)
        for key,version in [(self.pin['stock_package_key'],self.pin['stock_version']),(self.pin['new_package_key'],self.pin['new_version'])]:
            path=self.bundle/'payload'/key
            recipe_require(command('dpkg-deb','-f',str(path),'Package').strip()=='upower','wrong local package')
            recipe_require(command('dpkg-deb','-f',str(path),'Version').strip()==version,'wrong local package version')
        self.removed=unconfigured_sssd_links(Path('/etc/sssd/sssd.conf'),Path('/etc/sssd/conf.d'),self.pin['unconfigured_sssd_symlinks'])
        (self.backup/'host-policy-before.json').write_text(json.dumps({'sssd_symlinks':self.removed,'thresholds':self.before_thresholds},indent=2)+'\n')

    def apply(self):
        for name,target in self.removed:Path(name).unlink()
        command('udevadm','verify','/etc/udev/rules.d/99-sp11-battery-reporting.rules')
        command('udevadm','verify','/etc/udev/rules.d/85-brltty.rules')
        command('systemctl','daemon-reload');command('udevadm','control','--reload')
        command('systemd-analyze','verify','upower.service','sp11-battery-reporting-prepare.service')
        command('systemctl','start','sp11-battery-reporting-prepare.service')
        self.changed=True  # A package failure can still have changed its installed state.
        path=Path('/')/self.pin['new_package_target']
        command('dpkg','--force-confold','-i',str(path))
        pid=command('systemctl','show','upower.service','-p','MainPID','--value').strip()
        if pid=='0' or recipe_sha(Path('/proc')/pid/'exe')!=self.pin['daemon_sha256']:
            command('systemctl','restart','upower.service')
        recipe_require(recipe_sha(Path('/usr/libexec/upowerd'))==self.pin['daemon_sha256'],'UPower daemon differs')
        recipe_require(command('dpkg-query','-W','-f=${Version}','libupower-glib3').strip()==self.pin['library_version'],'UPower library changed')
        recipe_require(recipe_sha(Path('/etc/UPower/UPower.conf'))==self.pin['upower_config_sha256'],'UPower settings changed')
        recipe_require(thresholds()==self.before_thresholds,'charging thresholds changed')

    def rollback_begin(self):
        subprocess.run(['systemctl','stop','sp11-battery-reporting-prepare.service'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)

    def rollback_finish(self):
        for name,target in self.removed:
            path=Path(name)
            if not path.exists() and not path.is_symlink():path.symlink_to(target)
        command('systemctl','daemon-reload');command('udevadm','control','--reload')
        if self.changed:
            command('dpkg','--force-confold','-i',str(self.stock))
            command('systemctl','try-restart','upower.service')


def verify_live(pin):
    recipe_require(Path('/sys/kernel/security/apparmor/.notify').exists(),'AppArmor notification interface missing')
    features=command('snap','debug','sandbox-features')
    recipe_require('support-level:full' in features and command('snap','debug','confinement').strip()=='strict','Snap compatibility incomplete')
    devices=command('upower','--enumerate')
    recipe_require('battery_BAT1' in devices and 'battery_qcom_battmgr_bat' not in devices,'battery reporting duplicated or missing')
    recipe_require(recipe_sha(Path('/usr/libexec/upowerd'))==pin['daemon_sha256'],'patched UPower daemon missing')
    log=command('journalctl','-b','--no-pager','-o','cat')
    for marker in ["unrecognized option '--ignore-install'",'Error running install command','CMD timeout for [1001021]','unexpected descriptor length']:
        recipe_require(marker not in log,'startup integration error: '+marker)
    recipe_require('QCA setup on UART is completed' in log,'Bluetooth firmware setup incomplete')


def recipe_require(condition,message):
    if not condition:raise ValueError(message)


def recipe_sha(path):
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest()
