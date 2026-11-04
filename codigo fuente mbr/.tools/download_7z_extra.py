import hashlib
import pathlib
import urllib.request


url = "https://github.com/ip7z/7zip/releases/download/26.05/7z2605-extra.7z"
expected = "f27a8f103c84785026edcba1f67faf1357225df9d19ddb0b7760d55947141bc7"
size = 1763900
destination = pathlib.Path(__file__).parent / "downloads" / "7z2605-extra.7z"
chunk_size = 1024 * 1024
with destination.open("wb") as output:
    for start in range(0, size, chunk_size):
        end = min(start + chunk_size - 1, size - 1)
        request = urllib.request.Request(
            url,
            headers={
                "User-Agent": "VOS-local-build-tools/1.0",
                "Range": f"bytes={start}-{end}",
            },
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            content_range = response.headers.get("Content-Range", "")
            if response.status != 206 or content_range != f"bytes {start}-{end}/{size}":
                raise SystemExit(f"Unexpected response: {response.status} {content_range}")
            data = response.read()
        if len(data) != end - start + 1:
            raise SystemExit(f"Incomplete range {start}-{end}")
        output.write(data)
        print(f"Downloaded through {end + 1}/{size} bytes", flush=True)

actual = hashlib.sha256(destination.read_bytes()).hexdigest()
print(f"SHA256 {actual}")
if actual != expected:
    destination.unlink(missing_ok=True)
    raise SystemExit("7-Zip SHA256 mismatch")
