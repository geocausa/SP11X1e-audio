# AudioReach topology inventory

- Source: `/home/geoca/Documents/SP11-PROJECT/05-audio-integration/artifacts/cps-lab-candidate-20260810/X1E80100-Microsoft-Surface-Pro-11-CPS-Lab-tplg.bin`
- SHA-256: `f385a5d83127cf8f83dab0cbc86f418514f9c8839f2da6aac97e3e2ee782d121`
- Widgets: 30
- Modules: 29
- Raw-byte modules: 4

| Widget | Module | IID | SG | Container | Token destination | DAPM | Payloads | Issues |
|---|---|---:|---:|---:|---:|:---:|---:|---|
| sp11.sal.4001 | SAL `0x07001010` | `0x00004001` | `0xb0000001` | `0xe0000001` | `0x0000402c` | no | 0 |  |
| sp11.splitter.4002 | SPLITTER `0x07001011` | `0x00004002` | `0xb0000001` | `0xe0000001` | `0x00004003` | no | 0 |  |
| sp11.data_logging.4003 | DATA_LOGGING `0x0700101a` | `0x00004003` | `0xb0000001` | `0xe0000001` | `0x00004157` | no | 0 |  |
| sp11.speaker_protection.4027 | SPEAKER_PROTECTION `0x070010e2` | `0x00004027` | `0xb0000001` | `0xe0000001` | `0x00004002` | no | 3 |  |
| sp11.chmixer.402c | CHMIXER `0x07001013` | `0x0000402c` | `0xb0000001` | `0xe0000001` | `0x00004027` | no | 0 |  |
| sp11.codec_dma_sink.4157 | CODEC_DMA_SINK `0x07001023` | `0x00004157` | `0xb0000001` | `0xe0000001` | `None` | no | 0 |  |
| sp11.data_logging.402a | DATA_LOGGING `0x0700101a` | `0x0000402a` | `0xb0000001` | `0xe0000005` | `0x00004029` | no | 0 |  |
| sp11.codec_dma_source.402b | CODEC_DMA_SOURCE `0x07001024` | `0x0000402b` | `0xb0000001` | `0xe0000005` | `0x0000402a` | no | 0 |  |
| sp11.unknown_0xe4.4028 | UNKNOWN `0x070010e4` | `0x00004028` | `0xb0000001` | `0xe0000006` | `None` | no | 0 |  |
| sp11.mux_demux.4029 | MUX_DEMUX `0x07001098` | `0x00004029` | `0xb0000001` | `0xe0000006` | `0x00004028` | no | 0 |  |
| sp11.speaker_protection_vi.4024 | SPEAKER_PROTECTION_VI `0x070010e3` | `0x00004024` | `0xb0000001` | `0xe0000007` | `None` | no | 2 |  |
| sp11.data_logging.4025 | DATA_LOGGING `0x0700101a` | `0x00004025` | `0xb0000001` | `0xe0000007` | `0x00004024` | no | 0 |  |
| sp11.codec_dma_source.4026 | CODEC_DMA_SOURCE `0x07001024` | `0x00004026` | `0xb0000001` | `0xe0000007` | `0x00004025` | no | 0 |  |
| sp11.data_logging.465c | DATA_LOGGING `0x0700101a` | `0x0000465c` | `0xb000007e` | `0xe000004c` | `0x0000465f` | no | 0 |  |
| sp11.pcm_cnv.465f | PCM_CNV `0x07001003` | `0x0000465f` | `0xb000007e` | `0xe000004c` | `0x00004663` | no | 0 |  |
| sp11.sh_mem_pull_mode.4660 | SH_MEM_PULL_MODE `0x07001006` | `0x00004660` | `0xb000007e` | `0xe000004c` | `0x0000465c` | no | 2 |  |
| sp11.swr_sink.4662 | SWR_SINK `0x07001097` | `0x00004662` | `0xb000007e` | `0xe000004c` | `0x00004664` | no | 0 |  |
| sp11.vol_ctrl.4663 | VOL_CTRL `0x0700101b` | `0x00004663` | `0xb000007e` | `0xe000004c` | `0x00004662` | no | 0 |  |
| sp11.popless_equalizer.4664 | POPLESS_EQUALIZER `0x07001045` | `0x00004664` | `0xb000007e` | `0xe000004c` | `0x00004669` | no | 0 |  |
| sp11.vol_ctrl.4669 | VOL_CTRL `0x0700101b` | `0x00004669` | `0xb000007e` | `0xe000004c` | `0x0000466a` | no | 0 |  |
| sp11.mfc.466a | MFC `0x07001015` | `0x0000466a` | `0xb000007e` | `0xe000004c` | `0x0000466b` | no | 0 |  |
| sp11.soft_pause.466b | SOFT_PAUSE `0x07001019` | `0x0000466b` | `0xb000007e` | `0xe000004c` | `0x0000412b` | no | 0 |  |
| sp11.spr.412b | SPR `0x07001032` | `0x0000412b` | `0xb000007e` | `0xe0000066` | `0x000047e9` | no | 0 |  |
| sp11.swr_sink.4675 | SWR_SINK `0x07001097` | `0x00004675` | `0xb000007f` | `0xe0000114` | `0x0000489e` | no | 0 |  |
| sp11.data_logging.467a | DATA_LOGGING `0x0700101a` | `0x0000467a` | `0xb000007f` | `0xe0000114` | `0x00004001` | no | 0 |  |
| sp11.data_logging.47e9 | DATA_LOGGING `0x0700101a` | `0x000047e9` | `0xb000007f` | `0xe0000114` | `0x00004a63` | no | 0 |  |
| sp11.msiir.489e | MSIIR `0x07001014` | `0x0000489e` | `0xb000007f` | `0xe0000114` | `0x000048a1` | no | 0 |  |
| sp11.msiir.48a1 | MSIIR `0x07001014` | `0x000048a1` | `0xb000007f` | `0xe0000114` | `0x0000467a` | no | 0 |  |
| sp11.vol_ctrl.4a63 | VOL_CTRL `0x0700101b` | `0x00004a63` | `0xb000007f` | `0xe0000114` | `0x00004675` | no | 4 |  |

## Structural issues

No duplicate module instance IDs detected.
