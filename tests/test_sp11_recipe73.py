import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

PATH = Path(__file__).resolve().parents[1]/'recipes/sp11-7.3/recipe.py'
spec = importlib.util.spec_from_file_location('recipe73', PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PortableRecipe(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.recipe = self.base/'recipe'
        self.recipe.mkdir()
        self.bundle = self.base/'private-bundle'
        (self.bundle/'payload').mkdir(parents=True)
        (self.bundle/'kernel-source.tar.zst').write_bytes(b'fixture source archive')
        (self.bundle/'kernel-integration.mbox').write_bytes(b'fixture patches')
        payloads = {'module': b'module bytes', 'config': b'HOME=@HOME@ UID=@UID@\n', 'image': b'kernel bytes'}
        for name, data in payloads.items():
            (self.bundle/'payload'/name).write_bytes(data)
        # Keep the fixture module's normal layout, not a host module tree.
        target = self.bundle/'payload/modules/kernel/gpi.ko'
        target.parent.mkdir(parents=True)
        (self.bundle/'payload/module').rename(target)
        index = hashlib.sha256(payloads['module']).hexdigest()+'  kernel/gpi.ko\n'
        (self.recipe/'module-files.sha256').write_text(index)
        lock = {'recipe_id': 'fixture', 'release': '7.3.0-test', 'integration_commit': 'fixture',
                'source_artifacts': {name: module.sha(self.bundle/name) for name in ['kernel-source.tar.zst', 'kernel-integration.mbox']},
                'module_count': 1, 'module_map_sha256': hashlib.sha256(index.encode()).hexdigest(),
                'module_metadata': {}, 'kernel_args': ['ro', 'soundwire_qcom.sp11_cps_pcm_route_105c=1'],
                'private_kernel_arg': 'mshw0485_touch.parity_report56_identity',
                'files': [{'key': 'config', 'scope': 'user', 'target': '.config/graph.conf',
                           'template': True, 'mode': 0o644, 'sha256': hashlib.sha256(payloads['config']).hexdigest()},
                          {'key': 'image', 'scope': 'boot', 'target': 'vmlinuz-7.3.0-test',
                           'mode': 0o644, 'sha256': hashlib.sha256(payloads['image']).hexdigest()}]}
        (self.recipe/'release-lock.json').write_text(json.dumps(lock))
        self.tool = module.Recipe(self.recipe)
        self.metadata = {'recipe_id': 'fixture', 'release': '7.3.0-test',
                         'lock_sha256': module.sha(self.recipe/'release-lock.json'),
                         'touch_identity': '0x01,0x02,0x03,0x04,0x05,0x06',
                         'device_id_sha256': '0'*64}
        self.save_metadata()

    def save_metadata(self):
        (self.bundle/'bundle.json').write_text(json.dumps(self.metadata))

    def test_bundle_integrity(self):
        self.assertEqual(self.tool.verify(self.bundle)['recipe_id'], 'fixture')

    def test_changed_kernel_refused(self):
        (self.bundle/'payload/image').write_bytes(b'different kernel')
        with self.assertRaisesRegex(ValueError, 'payload mismatch'):
            self.tool.verify(self.bundle)

    def test_missing_private_identity_refused(self):
        self.metadata['touch_identity'] = ''
        self.save_metadata()
        with self.assertRaisesRegex(ValueError, 'private touch identity'):
            self.tool.verify(self.bundle)

    def test_newer_release_refused(self):
        self.metadata['release'] = '7.4.0-test'
        self.save_metadata()
        with self.assertRaisesRegex(ValueError, 'unqualified kernel'):
            self.tool.verify(self.bundle)

    def test_staging_relocates_home_uid_and_root(self):
        root = self.base/'new-install'
        manifest = self.tool.populate(self.bundle, root, '/home/another-owner', 2000,
                                      '11111111-2222-3333-4444-555555555555')
        self.assertEqual((root/'home/another-owner/.config/graph.conf').read_text(),
                         'HOME=/home/another-owner UID=2000\n')
        grub = (root/'etc/grub.d/79_sp11_baseline73').read_text()
        self.assertIn('root=UUID=11111111-2222-3333-4444-555555555555', grub)
        self.assertNotIn('trace_event=', grub)
        self.assertEqual(len(manifest['files']), 3)

    def test_stage_cannot_write_to_live_root(self):
        with self.assertRaisesRegex(ValueError, 'must not be /'):
            self.tool.populate(self.bundle, Path('/'), '/home/owner', 1000, '1'*36)

    def test_stage_refuses_existing_tree(self):
        root = self.base/'occupied'
        root.mkdir()
        (root/'keep').write_text('user work')
        with self.assertRaisesRegex(ValueError, 'must be empty'):
            self.tool.populate(self.bundle, root, '/home/owner', 1000, '1'*36)
        self.assertEqual((root/'keep').read_text(), 'user work')

    def test_payload_symlink_escape_refused(self):
        path = self.bundle/'payload/image'
        path.unlink()
        path.symlink_to(self.base/'outside')
        with self.assertRaisesRegex(ValueError, 'escapes root'):
            self.tool.verify(self.bundle)

    def test_failed_export_refused(self):
        (self.bundle/'EXPORT-FAILED').touch()
        with self.assertRaisesRegex(ValueError, 'incomplete export'):
            self.tool.verify(self.bundle)


if __name__ == '__main__':
    unittest.main()
