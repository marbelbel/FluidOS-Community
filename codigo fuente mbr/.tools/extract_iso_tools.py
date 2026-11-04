import pathlib, tarfile, zipfile
root = pathlib.Path(__file__).parent
limine_archive = root / 'downloads' / 'limine-binary.tar.xz'
limine_dir = root.parent / 'limine-binary'
limine_dir.mkdir(parents=True, exist_ok=True)
with tarfile.open(limine_archive, 'r:xz') as archive:
    for member in archive.getmembers():
        target = (limine_dir / member.name).resolve()
        if limine_dir.resolve() not in target.parents and target != limine_dir.resolve():
            raise RuntimeError(f'Unsafe archive member: {member.name}')
    archive.extractall(limine_dir)
wheel_path = root / 'downloads' / 'pycdlib-1.22.0-py3-none-any.whl'
package_dir = root / 'pycdlib'
package_dir.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(wheel_path) as archive:
    for name in archive.namelist():
        if name.startswith('pycdlib/') and not name.endswith('/'):
            archive.extract(name, package_dir)
print('Limine files:')
for p in sorted(limine_dir.rglob('*')):
    if p.is_file() and (p.name.lower().startswith('limine') or p.name.lower() in {'bootx64.efi'}):
        print(p.relative_to(limine_dir), p.stat().st_size)
print('PyCdlib package:', package_dir)
