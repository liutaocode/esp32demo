# ESP-IDF enumerates this project-local directory to include the shared hook.
idf_component_register(
    SRCS "../../../../bootloader_components/recovery_boot_hook/recovery_boot_hook.c"
    PRIV_REQUIRES bootloader_support
    PRIV_INCLUDE_DIRS "$ENV{IDF_PATH}/components/bootloader_support/private_include")
