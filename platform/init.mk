### FIRST STAGE INIT
PRODUCT_PACKAGES += \
    fstab.exynos9820_ramdisk

### INIT
PRODUCT_PACKAGES += \
    init.exynos9820.root.rc \
    init.exynos9820.rc \
    init.exynos9820.usb.rc \
    init.vendor.rilcommon.rc \
    fstab.exynos9820 \
    ueventd.exynos9820.rc
