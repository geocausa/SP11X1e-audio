#!/usr/bin/env python3
"""Rebuild the exact FullIO v19c initrd from Golden-v33 plus three LPASS modules."""
from __future__ import annotations
import argparse, hashlib, os, subprocess, tempfile
from pathlib import Path

GOLDEN_INITRD_SHA = '19db416046a363821f1d0887a43562d69c3593f6df85b7b16017adcc6bc59a44'
ACCEPTED_RAW_CPIO_SHA = '4e9b79709fc06c837f284fd325c49c237ad2691bde4f8cd2db81981d6e506afe'
ACCEPTED_INITRD_SHA = 'ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d'
KREL = '7.1.5-sp11-render-parity-v4+'
MODULES = {
    'snd-soc-lpass-macro-common': (
        '0698d60676385d7e1bd9459a8b57834809b4a8125e73c766425552687dd6683f',
        '26bedead38fdfad2a2e81df413b18c92eb0caebbf0dff9846294c398907a91f3',
    ),
    'snd-soc-lpass-va-macro': (
        '161fe5e40e48d6797821414cd0d2e31a91271084264ecdd288f502dd02ffeb47',
        'c7439f8588ef4af1f2c843812a3712b656639bbf850b23d528a2088dc0606a66',
    ),
    'snd-soc-lpass-tx-macro': (
        '19d4a65a03de6e120767874657072251b68e9383c8b2f637b3f912c22f1cd402',
        '4512aaff2d7ab37348c987e4e86163a80d8b753e725cf38c26e92148bfd13f3f',
    ),
}
FIELDS = ['ino','mode','uid','gid','nlink','mtime','devmajor','devminor','rdevmajor','rdevminor','check']


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()


def zstd_decompress(path: Path) -> bytes:
    return subprocess.check_output(['zstd','-q','-dc',str(path)])


def parse_newc(data: bytes):
    fields13=['ino','mode','uid','gid','nlink','mtime','filesize','devmajor','devminor','rdevmajor','rdevminor','namesize','check']
    off=0; out=[]
    while off+110 <= len(data):
        if data[off:off+6] != b'070701': raise ValueError(f'bad newc magic at {off}')
        vals=[]; p=off+6
        for _ in range(13): vals.append(int(data[p:p+8],16)); p += 8
        h=dict(zip(fields13,vals)); ns=h['namesize']
        name_b=data[off+110:off+110+ns-1]
        name=name_b.decode('utf-8','surrogateescape')
        dataoff=(off+110+ns+3)&~3
        payload=data[dataoff:dataoff+h['filesize']]
        off=(dataoff+h['filesize']+3)&~3
        out.append((name,h,payload))
        if name == 'TRAILER!!!': break
    if not out or out[-1][0] != 'TRAILER!!!': raise ValueError('newc trailer not found')
    return out


def load_manifest(path: Path):
    rows=[]
    with path.open('r',encoding='utf-8') as f:
        for line in f:
            line=line.rstrip('\n')
            if not line or line.startswith('#'): continue
            if line.startswith('name\t'): continue
            parts=line.split('\t')
            if len(parts) != 12: raise ValueError(f'bad manifest row: {line[:120]}')
            name=parts[0]; vals={k:int(v,16) for k,v in zip(FIELDS,parts[1:])}
            rows.append((name,vals))
    return rows


def append_newc(out: bytearray, name: str, meta: dict[str,int], payload: bytes):
    name_b=name.encode('utf-8','surrogateescape')+b'\0'
    vals=[meta['ino'],meta['mode'],meta['uid'],meta['gid'],meta['nlink'],meta['mtime'],len(payload),
          meta['devmajor'],meta['devminor'],meta['rdevmajor'],meta['rdevminor'],len(name_b),meta['check']]
    out.extend(b'070701'+''.join(f'{v:08X}' for v in vals).encode('ascii'))
    out.extend(name_b)
    out.extend(b'\0' * ((-len(out)) & 3))
    out.extend(payload)
    out.extend(b'\0' * ((-len(out)) & 3))


def compress_module(ko: Path, dst: Path):
    subprocess.run(['zstd','-q','-19','-T0','-f',str(ko),'-o',str(dst)],check=True)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--golden-initrd',type=Path,required=True)
    ap.add_argument('--module-dir',type=Path,required=True,help='directory containing the three rebuilt raw .ko files')
    ap.add_argument('--manifest',type=Path,default=Path(__file__).with_name('cpio-header-manifest.tsv'))
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--keep-raw',type=Path)
    args=ap.parse_args()

    got=sha256_file(args.golden_initrd)
    if got != GOLDEN_INITRD_SHA: raise SystemExit(f'Golden initrd SHA mismatch: {got}')
    golden=parse_newc(zstd_decompress(args.golden_initrd))
    manifest=load_manifest(args.manifest)
    if [n for n,_,_ in golden] != [n for n,_ in manifest]: raise SystemExit('Golden archive order/name set differs from accepted manifest')
    payloads={n:p for n,_,p in golden}

    with tempfile.TemporaryDirectory(prefix='v19c-modzst-') as td:
        td=Path(td)
        for b,(raw_sha,zst_sha) in MODULES.items():
            ko=args.module_dir/f'{b}.ko'
            if sha256_file(ko) != raw_sha: raise SystemExit(f'raw module SHA mismatch: {ko}')
            zst=td/f'{b}.ko.zst'; compress_module(ko,zst)
            gotz=sha256_file(zst)
            if gotz != zst_sha: raise SystemExit(f'compressed module SHA mismatch {b}: {gotz}')
            arc=f'usr/lib/modules/{KREL}/kernel/sound/soc/codecs/{b}.ko.zst'
            payloads[arc]=zst.read_bytes()

        raw=bytearray()
        for name,meta in manifest: append_newc(raw,name,meta,payloads[name])
        # GNU cpio pads the complete newc archive to its default 512-byte block.
        raw.extend(b'\0' * ((-len(raw)) & 511))
        raw_sha=sha256_bytes(raw)
        if raw_sha != ACCEPTED_RAW_CPIO_SHA: raise SystemExit(f'raw cpio SHA mismatch: {raw_sha}')
        if args.keep_raw: args.keep_raw.write_bytes(raw)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        p=subprocess.Popen(['zstd','-q','-19','-T0','-o',str(args.output)],stdin=subprocess.PIPE)
        assert p.stdin is not None
        p.stdin.write(raw); p.stdin.close()
        rc=p.wait()
        if rc: raise SystemExit(f'zstd failed: {rc}')
    final=sha256_file(args.output)
    if final != ACCEPTED_INITRD_SHA: raise SystemExit(f'final initrd SHA mismatch: {final}')
    print(f'FullIO v19c initrd exact reproduction: PASS {final}')

if __name__ == '__main__': main()
