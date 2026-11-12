#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

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

def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None

lib_fixups: lib_fixups_user_type = {
    libs_proto_3_9_1: lib_fixup_vendorcompat,
    'libuuid': lib_fixup_vendor_suffix,
} # fmt: skip

blob_fixups: blob_fixups_user_type = {
    'vendor/etc/libnfc-nci.conf': blob_fixup()
        .regex_replace('/data/nfc', '/data/vendor/nfc'),
    'vendor/lib64/libssl-tm.so': blob_fixup()
        .replace_needed('libcrypto.so', 'libcrypto-tm.so'),
    'vendor/lib/liboemcrypto.so': blob_fixup()
        .add_needed('libshim_oemcrypto.so')
        .binary_regex_replace(b'fopen', b'kopen'),
    'vendor/lib64/libsec-ril.so': blob_fixup()
	.sig_replace('80 0E 40 F9 E1 03 16 AA 82 0C 80 52 E3 03 15 AA',
            '80 0E 40 F9 E1 03 16 AA 82 0C 80 52 08 00 80 D2')
        # SmsRespBuilder::BuildBroadcastSmsResponse() prefixes the cell
        # broadcast PDU with two 16 bit lengths (header and body), which is
        # Samsung's own format. AOSP expects a plain 3GPP TS 23.041 PDU in
        # newBroadcastSms(), so it parses the lengths as serial number and
        # message identifier and drops the alert. Return a pointer to the
        # header (RilData + 0x240 instead of 0x23c) and omit the four length
        # bytes from the reported size.
        .sig_replace('68 02 45 B9 15 F0 08 91',
            '68 02 45 B9 15 00 09 91')
        .sig_replace('28 01 08 0B 08 11 00 11',
            '28 01 08 0B 08 01 00 11'),
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
