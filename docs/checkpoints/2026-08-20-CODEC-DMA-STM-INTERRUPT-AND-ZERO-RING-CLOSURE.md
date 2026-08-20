# CODEC_DMA STM interrupt + zero-ring closure — 2026-08-20

Branch: `agent/psycho-bass-20260818`

## Question closed

Does Linux merely create/open HWD4, while the CODEC_DMA source never receives the normal DMA/STM hardware trigger?

**No.** The earlier 273-frame CPS run proves repeated normal HWD signal-triggered processing. The remaining failure is sample content, not absence of the source interrupt/process lifecycle.

## Runtime premise

The previously tested disposable `v31-cps-pcm-port-ctrl-105c-20260819` candidate performed the Windows-only controller writes:

- WSA controller `0x105c = 0x0005000f`;
- CPS master-port `0x1d54 = 0x00000003`.

During an acoustically proven 997 Hz render it produced:

- 273 `cmd16` tap3 audio packets;
- 192-byte CPS payload per packet;
- 0 / 273 nonzero packets;
- S32 RMS `0.0`, peak `0`.

The question was whether these packets could have been caused by command processing or downstream output-buffer returns instead of the HWD interrupt.

## CODEC_DMA has STM as its sole framework extension

Static qcadsp decompile of CODEC_DMA property handling at `FUN_b0598428` constructs exactly one required framework-extension ID:

```text
local_34[0] = 0x0A001003   // FWK_EXTN_STM
...
num_extensions = 1
```

No `FWK_EXTN_TRIGGER_POLICY` is advertised by CODEC_DMA.

## DATA_LOGGING does not add a trigger-policy extension

Recovered AudioReach `capi_data_logging_process_get_properties()` declares:

```c
uint32_t fwk_extn_ids_arr[] = { FWK_EXTN_PCM };
mod_prop.num_fwk_extns = 1;
```

Its supported interface extensions are only:

- `INTF_EXTN_PROP_PORT_DS_STATE`;
- `INTF_EXTN_DATA_PORT_OPERATION`.

It does not support `FWK_EXTN_TRIGGER_POLICY`.

## Container 0xe0000005 has no alternate source/data trigger

The deployed topology places only these source-chain modules in container `0xe0000005`:

```text
CODEC_DMA_SOURCE 0x402b  (0 inputs, 1 output)
   -> DATA_LOGGING 0x402a
   -> cross-container downstream 0x4029
```

Thus the source side has no external input queue. With neither module advertising a data-trigger-policy framework extension, `num_data_tpm == 0` for this source container.

## Generic-container behavior is explicit

Recovered AudioReach `gen_cntr_cmd_handler.c` explicitly avoids topology processing from command context for a signal-triggered container:

```c
// for signal triggered if we call process_frames, STM (EP) module will be called at wrong time.
if (!me_ptr->topo.flags.is_signal_triggered) {
    gen_cntr_data_process_frames(me_ptr);
} else {
    gen_cntr_wait_for_trigger(me_ptr);
}
```

`gen_cntr_wait_for_any_ext_trigger()` is even stronger for an active signal-triggered container with no data TPM:

```c
stop_mask |= gen_cntr_get_all_output_port_mask(me_ptr);
stop_mask |= gen_cntr_get_all_input_port_mask(me_ptr);
cu_stop_listen_to_mask(&me_ptr->cu, stop_mask);
return WAIT_FOR_TRIGGER; // listen to only signal trigger
```

Therefore an output-buffer return, input queue, or normal command cannot be the repeating trigger responsible for the 273 source-chain process cycles.

## Signal handler lifecycle

Recovered `gen_cntr_st_handler_island.c` performs the normal signal path in this order:

1. compare `raised_interrupt_counter` and `processed_interrupt_counter`;
2. clear the STM trigger signal;
3. set `curr_trigger = GEN_TOPO_SIGNAL_TRIGGER`;
4. invoke `update_stm_ts_fptr(stm_ts_ctxt_ptr, ...)`;
5. process the topology once for that signal trigger.

The process loop explicitly limits signal-trigger execution to one pass per signal event.

## qcadsp HWD callback wiring

CODEC_DMA HWD-open function `FUN_b05999e0` embeds:

```text
local_2c = FUN_b005d6d4
context  = CODEC_DMA instance state
```

into the HWD configuration passed through `FUN_b027baa8`, followed by HWD start/configure through `FUN_b027bd48`.

`FUN_b005d6d4(context, irq_bits)` is the HWD callback. For the normal bit-0 interrupt it:

- increments `**(context + 0x54)` — the raised-interrupt counter supplied by the framework;
- signals the framework via `FUN_b005c648` / `FUN_b005c228`.

The only static reference embedding `FUN_b005d6d4` is the CODEC_DMA HWD-open path above.

## Conclusion

The 273 tap3 packets require repeated execution of the CODEC_DMA source topology from its STM signal. In this container there is no data-trigger-policy module and the framework explicitly stops listening to external input/output queues while STM is active.

Therefore:

**Linux HWD4/CODEC_DMA source normal hardware signal interrupts are alive and repeatedly drive topology processing.**

The result does not require assuming an exact one-packet-per-interrupt diagnostic transport ratio; it proves repeated HWD signal-trigger events occurred. The payload delivered by those cycles is nevertheless all zero.

This closes these hypotheses:

- HWD4 opens but never interrupts;
- CODEC_DMA source never gets a normal STM trigger;
- the 273 tap3 packets were merely downstream buffer/command activity.

Updated fault boundary:

```text
WSA8845 CPS/VISENSE producer
 -> SoundWire feedback ports / WSA master
 -> [DATA ENTERING LPAIF WSA WRDMA IS ZERO OR MISROUTED]
 -> HWD4 WRDMA lifecycle + interrupts: LIVE
 -> CODEC_DMA_SOURCE process cycles: LIVE
 -> tap3/tap2 payload: ZERO
```

Do not spend another candidate on WRDMA interrupt enable/start unless new contradictory evidence appears.
