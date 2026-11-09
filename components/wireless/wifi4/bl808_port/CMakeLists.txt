# BL808 (M0) platform glue for the wifi4 MAC firmware blob.
# Adapted from Zephyr's BL808 Wi-Fi support (soc/bflb/bl808, Apache-2.0).
sdk_generate_library()

sdk_add_include_directories(include)
sdk_library_add_sources(bl808_wifi_port.c)

# Work around crashes inside the blob (see bl808_wifi_port.c) and keep the
# first RF calibration from being preempted.
sdk_add_link_options(
    -Wl,--wrap=rc_update_counters
    -Wl,--wrap=sta_mgmt_unregister
    -Wl,--wrap=rfc_init
)
