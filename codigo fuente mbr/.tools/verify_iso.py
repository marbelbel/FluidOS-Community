import io, pathlib, sys
sys.path.insert(0, str(pathlib.Path('.tools/pycdlib').resolve()))
import pycdlib
iso_path = pathlib.Path('build/VOS.iso').resolve()
iso = pycdlib.PyCdlib()
iso.open(str(iso_path))
try:
    for p in ['/LIMINE.CONF;1', '/LIMINE_BIOS.SYS;1', '/BOOT/KERNEL.ELF;1', '/BOOT/LIMINE_BIOS_CD.BIN;1', '/BOOT/LIMINE_UEFI_CD.BIN;1', '/EFI/BOOT/BOOTX64.EFI;1', '/BOOT.CAT;1']:
        buf = io.BytesIO()
        iso.get_file_from_iso_fp(buf, iso_path=p)
        print(f'{p}: {len(buf.getvalue())} bytes')
    catalog = iso.eltorito_boot_catalog
    print('El Torito catalog:', 'present' if catalog else 'missing')
    print('Boot image:', catalog.initial_entry.inode.data_length if catalog else 'n/a')
    print('Supplementary descriptors:', iso.joliet_vd is not None, 'Rock Ridge:', bool(iso.rock_ridge))
finally:
    iso.close()
