"""The real verifier must select the image release even on a different host."""
from pathlib import Path
from unittest import TestCase, mock
from recipe import Recipe


class TargetReleaseLoader(TestCase):
    def setUp(self):
        self.tool = Recipe.__new__(Recipe)
        self.tool.release = '7.3.0-image'

    def test_target_release_and_no_load(self):
        output = 'insmod /lib/modules/7.3.0-image/kernel/sound/core/snd.ko\n'
        with mock.patch('recipe.command', return_value=output) as command:
            self.assertEqual(self.tool.check_module_loader(Path('/scratch/image')), output)
        self.assertEqual(command.call_args.args[0], ['chroot', '/scratch/image',
            '/sbin/modprobe', '--set-version', '7.3.0-image', '--ignore-install',
            '--show-depends', 'snd'])

    def test_empty_success_rejected(self):
        with mock.patch('recipe.command', return_value=''):
            with self.assertRaises(ValueError): self.tool.check_module_loader(Path('/scratch'))

    def test_wrong_release_rejected(self):
        with mock.patch('recipe.command', return_value='insmod /lib/modules/7.1-host/snd.ko'):
            with self.assertRaises(ValueError): self.tool.check_module_loader(Path('/scratch'))

    def test_dependency_command_failure_propagates(self):
        from subprocess import CalledProcessError
        with mock.patch('recipe.command', side_effect=CalledProcessError(1, 'modprobe')):
            with self.assertRaises(CalledProcessError): self.tool.check_module_loader(Path('/scratch'))
