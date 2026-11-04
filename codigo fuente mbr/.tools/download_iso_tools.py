import concurrent.futures, hashlib, json, pathlib, urllib.request
root = pathlib.Path(__file__).parent
out = root / 'downloads'
out.mkdir(parents=True, exist_ok=True)
items = [
    ('https://github.com/limine-bootloader/limine/releases/download/v12.9.1/limine-binary.tar.xz', 'limine-binary.tar.xz', 856876, 'ce972a05e9d1973dc9b725f9130bd15af01add0eb3a7f90c118ac5cfe21c17b8'),
]
# Resolve the exact PyPI wheel and its published digest from PyPI metadata.
with urllib.request.urlopen('https://pypi.org/pypi/pycdlib/1.22.0/json', timeout=30) as response:
    metadata = json.load(response)
wheel = next(item for item in metadata['urls'] if item['filename'] == 'pycdlib-1.22.0-py3-none-any.whl')
items.append((wheel['url'], wheel['filename'], wheel['size'], wheel['digests']['sha256']))
chunk = 1024 * 1024

def fetch(item):
    url, name, size, digest = item
    path = out / name
    data_parts = []
    for start in range(0, size, chunk):
        end = min(start + chunk - 1, size - 1)
        request = urllib.request.Request(url, headers={'User-Agent':'VOS-local-build-tools/1.0','Range':f'bytes={start}-{end}'})
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read()
            if response.status == 206:
                expected_range = f'bytes {start}-{end}/{size}'
                if response.headers.get('Content-Range') != expected_range:
                    raise RuntimeError(f'{name}: unexpected Content-Range {response.headers.get("Content-Range")}')
            elif not (response.status == 200 and start == 0 and len(data) == size):
                raise RuntimeError(f'{name}: unexpected HTTP {response.status} ({len(data)} bytes)')
        if len(data) != end - start + 1:
            raise RuntimeError(f'{name}: incomplete block {start}-{end}: {len(data)}')
        data_parts.append(data)
    payload = b''.join(data_parts)
    actual = hashlib.sha256(payload).hexdigest()
    if actual.lower() != digest.lower():
        raise RuntimeError(f'{name}: SHA256 mismatch: {actual}')
    path.write_bytes(payload)
    return f'Verified {path} SHA256={actual} size={len(payload)}'

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    for result in pool.map(fetch, items):
        print(result, flush=True)

