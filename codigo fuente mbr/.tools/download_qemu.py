import hashlib
import pathlib
import urllib.request


url = "https://qemu.weilnetz.de/w64/2026/qemu-w64-setup-20260811.exe"
expected_sha512 = (
    "5bcf9eed634e8575a37b74f445af41a2fe4106da512d0c30c368301d4c10503"
    "7fdfab40a5287367a28a957624cddebbc8c07e16c88ab6634f554cdf3d16bf543"
)
destination = pathlib.Path(__file__).parent / "downloads" / "qemu-w64-setup-20260811.exe"
chunk_size = 2 * 1024 * 1024
destination.parent.mkdir(parents=True, exist_ok=True)


with destination.open("wb") as output:
    start = 0
    total = None
    while total is None or start < total:
        end = start + chunk_size - 1
        request = urllib.request.Request(
            url,
            headers={
                "User-Agent": "VOS-local-build-tools/1.0",
                "Range": f"bytes={start}-{end}",
            },
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            content_range = response.headers.get("Content-Range", "")
            if response.status != 206 or not content_range.startswith(f"bytes {start}-"):
                raise SystemExit(f"Range request was not honored: {response.status} {content_range}")
            total = int(content_range.rsplit("/", 1)[1])
            expected_bytes = min(end, total - 1) - start + 1
            block = response.read()
            if len(block) != expected_bytes:
                raise SystemExit(f"Incomplete range at byte {start}: {len(block)} bytes")
            output.write(block)
        start += expected_bytes
        if start == total or start % (10 * 1024 * 1024) < chunk_size:
            print(f"Downloaded {start}/{total} bytes", flush=True)

digest = hashlib.sha512()
with destination.open("rb") as stream:
    while block := stream.read(1024 * 1024):
        digest.update(block)
actual = digest.hexdigest()
print(f"SHA512 {actual}")
if actual != expected_sha512:
    destination.unlink(missing_ok=True)
    raise SystemExit("QEMU SHA512 did not match the publisher's checksum")
print(f"Verified QEMU installer: {destination.resolve()}")
