#
# SPDX-License-Identifier: Apache-2.0
#
# Soong config for building the Samsung System LSI BSP camera HAL
# (hardware/samsung_slsi-linaro/camera/N23) instead of using the stock blobs.
#

# String variables
$(call soong_config_set,exynos_camera,target_soc_base,exynos9820)
$(call soong_config_set,exynos_camera,back_camera_sensor,SENSOR_NAME_SAK2L4)
$(call soong_config_set,exynos_camera,back_1_camera_sensor,SENSOR_NAME_S5K3M5)
$(call soong_config_set,exynos_camera,back_2_camera_sensor,SENSOR_NAME_S5K3P9)
$(call soong_config_set,exynos_camera,front_camera_sensor,SENSOR_NAME_S5K3J1)
$(call soong_config_set,exynos_camera,back_camera_rotation,90)
$(call soong_config_set,exynos_camera,front_camera_rotation,270)
$(call soong_config_set,exynos_camera,secure_camera_rotation,0)

# Boolean variables
$(call soong_config_set_bool,exynos_camera,use_camera_exynos9820_meta,true)
$(call soong_config_set_bool,exynos_camera,uses_slsi_plugin,true)
$(call soong_config_set_bool,exynos_camera,uses_slsi_vendor_tags,true)
