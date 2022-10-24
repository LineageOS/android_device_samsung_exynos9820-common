#!/bin/env python3
#
# Copyright (C) 2021-2022 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import common
import re

def FullOTA_InstallEnd(info):
  OTA_InstallEnd(info)
  return

def IncrementalOTA_InstallEnd(info):
  info.input_zip = info.target_zip
  OTA_InstallEnd(info)
  return

def AddImage(info, basename, dest):
  data = info.input_zip.read("IMAGES/" + basename)
  common.ZipWriteStr(info.output_zip, basename, data)
  info.script.Print("Patching {} image unconditionally...".format(dest.split('/')[-1]))
  info.script.AppendExtra('package_extract_file("%s", "%s");' % (basename, dest))

def AddFirmwareImage(info, model, basename, dest, simple=False):
  if ("RADIO/%s_%s" % (basename, model)) in info.input_zip.namelist():
    data = info.input_zip.read("RADIO/%s_%s" % (basename, model))
    common.ZipWriteStr(info.output_zip, "firmware/%s/%s" % (model, basename), data);
    info.script.Print("Patching {} image unconditionally...".format(basename.split('.')[0]));
    if simple:
      info.script.AppendExtra('package_extract_file("firmware/%s/%s", "%s");' % (model, basename, dest))
    else:
      size = info.input_zip.getinfo("RADIO/%s_%s" % (basename, model)).file_size
      info.script.AppendExtra('assert(exynos9820.mark_header_bt("%s", 0, 0, 0));' % dest);
      info.script.AppendExtra('assert(exynos9820.write_data_bt("firmware/%s/%s", "%s", 8, %d));' % (model, basename, dest, size))
      info.script.AppendExtra('assert(exynos9820.mark_header_bt("%s", 0, 0, 3142939818));' % dest)

def OTA_InstallEnd(info):
  AddImage(info, "dtb.img", "/dev/block/by-name/dtb")
  AddImage(info, "dtbo.img", "/dev/block/by-name/dtbo")
  AddImage(info, "vbmeta.img", "/dev/block/by-name/vbmeta")

  if "RADIO/models" in info.input_zip.namelist():
    modelsIncluded = []
    for model in info.input_zip.read("RADIO/models").decode('utf-8').splitlines():
      if "RADIO/version_%s" % model in info.input_zip.namelist():
        modelsIncluded.append(model)
        version = info.input_zip.read("RADIO/version_%s" % model).decode('utf-8').splitlines()[0]
        info.script.AppendExtra('# Firmware update to %s for %s' % (version, model))
        info.script.AppendExtra('ifelse (getprop("ro.boot.em.model") == "%s" &&' % model)
        info.script.AppendExtra('exynos9820.verify_no_downgrade("%s") == "0" &&' % version)
        info.script.AppendExtra('getprop("ro.boot.bootloader") != "%s",' % version)
        AddFirmwareImage(info, model, "sboot.bin", "/dev/block/by-name/bota0")
        AddFirmwareImage(info, model, "cm.bin", "/dev/block/by-name/bota1")
        AddFirmwareImage(info, model, "up_param.bin", "/dev/block/by-name/bota2")
        AddFirmwareImage(info, model, "keystorage.bin", "/dev/block/by-name/keystorage", True)
        AddFirmwareImage(info, model, "uh.bin", "/dev/block/by-name/uh", True)
        AddFirmwareImage(info, model, "modem.bin", "/dev/block/by-name/radio", True)
        AddFirmwareImage(info, model, "modem_5g.bin", "/dev/block/by-name/radio2", True)
        AddFirmwareImage(info, model, "modem_debug.bin", "/dev/block/by-name/cp_debug", True)
        AddFirmwareImage(info, model, "modem_debug_5g.bin", "/dev/block/by-name/cp2_debug", True)
        AddFirmwareImage(info, model, "dqmdbg.bin" "/dev/block/by-name/dqmdbg", True)
        AddFirmwareImage(info, model, "param.bin" "/dev/block/by-name/param", True)
        info.script.AppendExtra(',"");')

    modelCheck = ""
    for model in modelsIncluded:
      if len(modelCheck) > 0:
        modelCheck += ' || '
      modelCheck += 'getprop("ro.boot.em.model") == "%s"' % model
    if len(modelCheck) > 0:
      info.script.AppendExtra('%s || abort("Unsupported model, not updating firmware!");' % modelCheck)
  return
