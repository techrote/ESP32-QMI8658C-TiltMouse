#!/usr/bin/env python3
"""Verify packet hashes, exact retained Git blobs and text sanity. No network."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parent
manifest=json.loads((root/'MANIFEST.json').read_text())
listed=set()
for entry in manifest['files']:
    rel=Path(entry['path'])
    assert not rel.is_absolute() and '..' not in rel.parts, f'unsafe path {rel}'
    data=(root/rel).read_bytes(); listed.add(rel.as_posix())
    assert len(data)==entry['bytes'],f'size: {rel}'
    assert hashlib.sha256(data).hexdigest()==entry['sha256'],f'hash: {rel}'
actual={x.relative_to(root).as_posix() for x in root.rglob('*') if x.is_file()}
assert actual==listed|{'MANIFEST.json'},f'unlisted/missing: {actual^(listed|{"MANIFEST.json"})}'
sources=json.loads((root/'evidence/source_verification.json').read_text())
for name,entry in sources['files'].items():
    data=(root/'evidence/assessed'/name).read_bytes()
    blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    assert blob==entry['expected_git_blob'],f'Git blob: {name}'
for name in sorted(listed|{'MANIFEST.json'}):
    data=(root/name).read_bytes()
    if data.startswith(b'\x7fELF'):continue
    text=data.decode('utf-8')
    assert '\r' not in text,f'CR: {name}'
    assert not text or text.endswith('\n'),f'newline: {name}'
    # Diff context blank lines intentionally have a single leading space.
    if not name.endswith('.patch'):
        assert all(x==x.rstrip() for x in text.splitlines()),f'trailing whitespace: {name}'
print(json.dumps({'files_sha256_verified':len(listed),'git_blobs_verified':len(sources['files']),
                  'text_sanity':'pass','network_access':False},indent=2))
