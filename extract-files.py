#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import struct

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixup_vendorcompat,
    lib_fixups_user_type,
    libs_proto_3_9_1,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/samsung/exynos9820-common',
    'hardware/samsung',
    'hardware/samsung_slsi-linaro/graphics',
]

# The Exynos M4 cores lack FEAT_LRCPC, so LDAPR{,H,B} raise SIGILL there.
# LDAR is a stricter acquire load with identical operands and size, so it
# is a safe drop-in replacement. Maps LDAPR (mask 0xfffffc00) -> LDAR.
LDAPR_TO_LDAR = {
    0xF8BFC000: 0xC8DFFC00,
    0xB8BFC000: 0x88DFFC00,
    0x78BFC000: 0x48DFFC00,
    0x38BFC000: 0x08DFFC00,
}


def ldapr_to_ldar_impl(ctx, file, file_path: str, *args, **kwargs):
    with open(file_path, 'rb+') as f:
        data = bytearray(f.read())

        if data[:4] != b'\x7fELF' or data[4] != 2 or data[5] != 1:
            raise ValueError(f'{file_path}: not a 64-bit little-endian ELF')

        e_shoff, = struct.unpack_from('<Q', data, 0x28)
        e_shentsize, e_shnum = struct.unpack_from('<HH', data, 0x3A)

        count = 0
        for i in range(e_shnum):
            sh_type, sh_flags, _, sh_offset, sh_size = struct.unpack_from(
                '<IQQQQ', data, e_shoff + i * e_shentsize + 4
            )
            # SHT_PROGBITS (1) with SHF_EXECINSTR (0x4) only
            if sh_type != 1 or not sh_flags & 0x4:
                continue
            for off in range(sh_offset, sh_offset + sh_size - 3, 4):
                insn, = struct.unpack_from('<I', data, off)
                new = LDAPR_TO_LDAR.get(insn & 0xFFFFFC00)
                if new is not None:
                    struct.pack_into('<I', data, off, new | (insn & 0x3FF))
                    count += 1

        f.seek(0)
        f.write(data)

    print(f'ldapr_to_ldar: patched {count} instruction(s) in {file_path}')


def ldapr_to_ldar() -> blob_fixup:
    return blob_fixup().call(ldapr_to_ldar_impl, need_tmp_dir=False)


def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None

lib_fixups: lib_fixups_user_type = {
    libs_proto_3_9_1: lib_fixup_vendorcompat,
    'libuuid': lib_fixup_vendor_suffix,
} # fmt: skip

blob_fixups: blob_fixups_user_type = {
    'vendor/bin/hw/rild': ldapr_to_ldar(),
    'vendor/etc/libnfc-nci.conf': blob_fixup()
        .regex_replace('/data/nfc', '/data/vendor/nfc'),
    'vendor/lib64/libssl-tm.so': blob_fixup()
        .replace_needed('libcrypto.so', 'libcrypto-tm.so'),
    'vendor/lib/liboemcrypto.so': blob_fixup()
        .add_needed('libshim_oemcrypto.so')
        .binary_regex_replace(b'fopen', b'kopen'),
    'vendor/lib64/libsec-ril.so': blob_fixup()
	.sig_replace('80 0E 40 F9 E1 03 16 AA 82 0C 80 52 E3 03 15 AA',
            '80 0E 40 F9 E1 03 16 AA 82 0C 80 52 08 00 80 D2'),
    'vendor/lib64/libsensorlistener.so': blob_fixup()
        .add_needed('libshim_sensorndkbridge.so'),
    (
        'vendor/lib64/libkeymaster_helper.so',
        'vendor/lib64/libskeymaster4device.so',
    ): blob_fixup()
        .replace_needed('libcrypto.so', 'libcrypto-tm.so')
        .add_needed('libshim_crypto.so'),
    'vendor/lib/libwvhidl.so': blob_fixup()
        .replace_needed('libprotobuf-cpp-lite-3.9.1.so', 'libprotobuf-cpp-full-3.9.1.so'),
    (
        'vendor/lib/sensors.grip.so',
        'vendor/lib64/sensors.grip.so',
    ): blob_fixup()
        .add_needed('libutils-v32.so')
        .binary_regex_replace(b'_ZN7android6Thread3runEPKcim', b'_ZN7utils326Thread3runEPKcim'),
}  # fmt: skip

module = ExtractUtilsModule(
    'exynos9820-common',
    'samsung',
    namespace_imports=namespace_imports,
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
