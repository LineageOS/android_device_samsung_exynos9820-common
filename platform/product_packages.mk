### AUDIO
PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-impl:32 \
    android.hardware.audio@5.0-impl:32 \
    android.hardware.audio@2.0-service

PRODUCT_PACKAGES += \
    libaudioroute \
    libtinyalsa \
    libtinycompress

PRODUCT_PACKAGES += \
    audio.r_submix.default \
    audio.usb.default

PRODUCT_PACKAGES += \
    android.hardware.audio.effect@2.0-impl:32 \
    android.hardware.audio.effect@5.0-impl:32

### BLUETOOTH
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-impl:64 \
    android.hardware.bluetooth@1.0-service

PRODUCT_PACKAGES += \
    audio.a2dp.default \
    audio.bluetooth.default \
    android.hardware.bluetooth.a2dp@1.0 \
    android.hardware.bluetooth.audio@2.0 \
    android.hardware.bluetooth.audio@2.0-impl:32

### CAMERA
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4 \
    android.hardware.camera.provider@2.5 \
    android.hardware.camera.provider@2.4-legacy \
    android.hardware.camera.provider@2.5-legacy

PRODUCT_PACKAGES += \
    Snap

### DOZE
PRODUCT_PACKAGES += \
    SamsungDoze

### DRM
PRODUCT_PACKAGES += \
    android.hardware.drm@1.3-service.clearkey

### BIOMETRICS
PRODUCT_PACKAGES += \
    android.hardware.biometrics.face@1.0

### FINGERPRINT
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.1-service.samsung

### GATEKEEPER
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl:64 \
    android.hardware.gatekeeper@1.0-service

### GRAPHICS
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl:64 \
    android.hardware.graphics.allocator@2.0-service \

PRODUCT_PACKAGES += \
    android.hardware.graphics.composer@2.2-service

PRODUCT_PACKAGES += \
    android.hardware.graphics.mapper@2.0-impl \
    android.hardware.graphics.mapper@2.0-impl-2.1

### HEALTH
PRODUCT_PACKAGES += \
    android.hardware.health@2.1-impl:64 \
    android.hardware.health@2.1-service

### KEYMASTER
PRODUCT_PACKAGES += \
    android.hardware.keymaster@4.0-service.samsung \
    libkeymaster4_1support.vendor:64

### LIGHT
PRODUCT_PACKAGES += \
    android.hardware.light@2.0-service.samsung

### MEMTRACK
PRODUCT_PACKAGES += \
    android.hardware.memtrack@1.0-impl:64 \
    android.hardware.memtrack@1.0-service

### NEURALNETWORKS
PRODUCT_PACKAGES += \
    android.hardware.neuralnetworks@1.0 \
    android.hardware.neuralnetworks@1.1 \
    android.hardware.neuralnetworks@1.2

PRODUCT_PACKAGES += \
    libtextclassifier_hash.vendor

### NFC
PRODUCT_PACKAGES += \
    com.android.nfc_extras \
    NfcNci \
    Tag

### POWER
PRODUCT_PACKAGES += \
    android.hardware.power@1.3-service.samsung-libperfmgr \
    libperfmgr.vendor:64

### RENDERSCRIPT
PRODUCT_PACKAGES += \
    android.hardware.renderscript@1.0-impl

### SENSORS
PRODUCT_PACKAGES += \
    android.hardware.sensors@2.1-service.samsung-multihal

PRODUCT_PACKAGES += \
    libsensorndkbridge

### SHIMS
PRODUCT_PACKAGES += \
    libshim_sensorndkbridge

### SOUNDTRIGGER
PRODUCT_PACKAGES += \
    android.hardware.soundtrigger@2.0-impl:32

### THERMAL
PRODUCT_PACKAGES += \
    android.hardware.thermal@2.0-service.samsung

### TOUCH HAL
PRODUCT_PACKAGES += \
    vendor.lineage.touch@1.0-service.samsung

### USB
PRODUCT_PACKAGES += \
    android.hardware.usb@1.1-service.exynos9820

### USB TRUST HAL
PRODUCT_PACKAGES += \
    vendor.lineage.trust@1.0-service

### VIBRATOR
PRODUCT_PACKAGES += \
    android.hardware.vibrator@1.3-service.samsung

### WIFI
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.0-service \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf
