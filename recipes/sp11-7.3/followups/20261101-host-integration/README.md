# 7.3 host-integration follow-up: APM initial query

The full unified12 kernel now passed two owner-confirmed sound/touch boots.
Use the complete versioned `../../../sp11-7.3-host-integration/` recipe for
installation; this source follow-up alone is not an installer.
The accepted parent recipe/lock remains unchanged.

The GPR bus installs the service callback after APM probe returns. An ignored
synchronous state query inside probe cannot receive its reply. Remove only that
query; later q6apm_is_adsp_ready() and q6prm's defer-on-not-ready gate remain.
Speaker protection, calibration and stream sequencing are unchanged.

Run the actual-C regression cases against the pre-fix source:

```sh
SP11_KERNEL_SOURCE=/path/to/pre-fix/linux python3 -m unittest discover -s recipes/sp11-7.3/followups/20261101-host-integration -p 'test_*.py' -v
```

Seven cases reproduce the old dropped reply, preserve later successful readiness,
not-ready/transport/timeout handling, and preserve probe allocation/component
failures. Apply the patch to the kernel integration tree, then compile and qualify
the whole candidate before changing any accepted boot default. This follow-up
does not replace the complete locked kernel/userspace/private-firmware recipe.
