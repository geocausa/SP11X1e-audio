"""Failure recovery and configured-domain protection, without a real system bus."""
from pathlib import Path
from types import SimpleNamespace
from unittest import TestCase,mock
import subprocess,tempfile
import host_policy as hp


class HostRecovery(TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.domain=self.root/'sssd.conf';self.fragments=self.root/'conf.d'
        self.link=self.root/'sssd-pam.socket';self.target='/usr/lib/systemd/system/sssd-pam.socket'
        self.links={str(self.link):self.target};self.link.symlink_to(self.target)

    def test_configured_domain_is_preserved(self):
        self.domain.write_text('[sssd]\n')
        self.assertEqual(hp.unconfigured_sssd_links(self.domain,self.fragments,self.links),[])
        self.assertTrue(self.link.is_symlink())

    def test_config_fragment_is_preserved(self):
        self.fragments.mkdir();(self.fragments/'domain.conf').write_text('[domain/example]\n')
        self.assertEqual(hp.unconfigured_sssd_links(self.domain,self.fragments,self.links),[])

    def test_unexpected_enablement_is_refused(self):
        self.link.unlink();self.link.write_text('user file')
        with self.assertRaisesRegex(ValueError,'unexpected SSSD'):
            hp.unconfigured_sssd_links(self.domain,self.fragments,self.links)
        self.assertEqual(self.link.read_text(),'user file')

    def test_partial_package_failure_keeps_stock_and_restores_enablement(self):
        bundle=self.root/'bundle';(bundle/'payload/packages').mkdir(parents=True)
        for name in ['stock','new']:(bundle/'payload/packages'/name).write_bytes(name.encode())
        backup=self.root/'backup';backup.mkdir();events=[]
        pin={'preconditions':{},'stock_package_key':'packages/stock','new_package_key':'packages/new',
             'new_package_target':'var/lib/sp11-test/new.deb','stock_version':'stock','new_version':'new',
             'unconfigured_sssd_symlinks':self.links}
        recipe=SimpleNamespace(lock={'host_policy':pin})
        def command(*args):
            events.append(args)
            if args[0]=='dpkg-deb':return 'upower\n' if args[-1]=='Package' else Path(args[2]).name+'\n'
            if args[0]=='dpkg' and args[-1]=='/var/lib/sp11-test/new.deb':
                raise subprocess.CalledProcessError(1,args,output='simulated partial package failure')
            return ''
        original=hp.unconfigured_sssd_links
        def local_links(domain,fragments,links):return original(self.domain,self.fragments,links)
        with mock.patch.object(hp,'command',side_effect=command),mock.patch.object(hp,'thresholds',return_value={}),mock.patch.object(hp,'unconfigured_sssd_links',side_effect=local_links):
            host=hp.HostPolicy(recipe,bundle,backup)
            with self.assertRaises(subprocess.CalledProcessError):host.apply()
            self.assertTrue(host.changed)
            self.assertFalse(self.link.is_symlink())
            host.rollback_finish()
        self.assertEqual(self.link.readlink(),Path(self.target))
        self.assertEqual((backup/'stock-upower.deb').read_bytes(),b'stock')
        self.assertIn(('dpkg','--force-confold','-i',str(backup/'stock-upower.deb')),events)
