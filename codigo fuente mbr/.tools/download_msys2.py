import hashlib
import pathlib
import urllib.request


release = "https://github.com/msys2/msys2-installer/releases/download/2026-09-27/"
filename = "msys2-base-x86_64-20260927.tar.xz"
downloads = pathlib.Path(__file__).parent / "downloads"
downloads.mkdir(parents=True, exist_ok=True)


def download(name: str) -> pathlib.Path:
    destination = downloads / name
    request = urllib.request.Request(
        release + name,
        headers={"User-Agent": "VOS-local-build-tools/1.0"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        with destination.open("wb") as output:
            while block := response.read(1024 * 1024):
                output.write(block)
    return destination


checksum_file = download(filename + ".sha256")
expected = checksum_file.read_text(encoding="ascii").split()[0].lower()
archive = downloads / filename
url = release + filename
chunk_size = 2 * 1024 * 1024
with archive.open("wb") as output:
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
        print(f"Downloaded {start}/{total} bytes", flush=True)
digest = hashlib.sha256()
with archive.open("rb") as stream:
    while block := stream.read(1024 * 1024):
        digest.update(block)
actual = digest.hexdigest()
print(f"SHA256 {actual}")
if actual != expected:
    archive.unlink(missing_ok=True)
    raise SystemExit(f"Checksum mismatch; expected {expected}")
print(f"Verified official MSYS2 archive: {archive}")
