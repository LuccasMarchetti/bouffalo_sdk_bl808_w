# BL808 PHY/RF: calibration blob shared by Wi-Fi and BLE, plus rfparam_init().
# The blob is not redistributed: run tools/bl808_wireless/fetch_blobs.sh.
if(NOT "${CPU_ID}" STREQUAL "m0")
    message(FATAL_ERROR "BL808 RF (CONFIG_RF) is only available on the M0 core (CPU_ID=m0).")
endif()

set(BL808_PHYRF_LIB ${BL_SDK_BASE}/components/wireless/bl808_blobs/lib/libbl606p_phyrf.a)
if(NOT EXISTS ${BL808_PHYRF_LIB})
    message(FATAL_ERROR "BL808 PHY/RF blob not found: ${BL808_PHYRF_LIB}\n"
                        "Run tools/bl808_wireless/fetch_blobs.sh first.")
endif()

sdk_generate_library(bl808_phyrf)
sdk_add_include_directories(include)
sdk_library_add_sources(bl808_rf.c)
sdk_add_static_library(${BL808_PHYRF_LIB})

# Keep the first RF calibration from being preempted (see bl808_rf.c).
sdk_add_link_options(-Wl,--wrap=rfc_init)
