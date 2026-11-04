import concurrent.futures
import hashlib
import pathlib
import urllib.request


url = "https://downloads.sourceforge.net/project/bochs/bochs/3.1/Bochs-win64-3.1.exe"
expected_sha256 = "ccbec06ec5baa81064cfe7c91862d0027b5033dbfa0b773d22e8be92094fdbb5"
size = 8029860
chunk_size = 1024 * 1024
destination = pathlib.Path(__file__).parent / "downloads" / "Bochs-win64-3.1.exe"
destination.parent.mkdir(parents=True, exist_ok=True)


def fetch(start: int) -> tuple[int, bytes]:
    end = min(start + chunk_size - 1, size - 1)
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "VOS-local-build-tools/1.0",
            "Range": f"bytes={start}-{end}",
        },
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        expected_range = f"bytes {start}-{end}/{size}"
        if response.status != 206 or response.headers.get("Content-Range") != expected_range:
            raise RuntimeError(f"Unexpected response for {start}-{end}: {response.status} {response.headers.get('Content-Range')}")
        block = response.read()
    if len(block) != end - start + 1:
        raise RuntimeError(f"Incomplete range at {start}: {len(block)} bytes")
    return start, block


parts = range(0, size, chunk_size)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    chunks = sorted(pool.map(fetch, parts), key=lambda item: item[0])
with destination.open("wb") as output:
    for start, block in chunks:
        output.write(block)
        print(f"Downloaded through {start + len(block)}/{size} bytes", flush=True)

actual_sha256 = hashlib.sha256(destination.read_bytes()).hexdigest()
print(f"SHA256 {actual_sha256}")
if actual_sha256 != expected_sha256:
    destination.unlink(missing_ok=True)
    raise SystemExit("Bochs SHA256 did not match SourceForge's checksum")
print(f"Verified Bochs installer: {destination.resolve()}")
