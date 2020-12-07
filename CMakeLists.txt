idf_component_register(	SRCS "log.c" "log_buffers.c"
						INCLUDE_DIRS "include"
						LDFRAGMENTS linker.lf
						PRIV_REQUIRES common soc printfx syslog stringsX
						)

idf_build_get_property(BUILD_COMPONENTS BUILD_COMPONENTS)
# Ideally, FreeRTOS shouldn't be included into bootloader build, so the 2nd check should be unnecessary
if(freertos IN_LIST BUILD_COMPONENTS AND NOT BOOTLOADER_BUILD)
    target_sources(${COMPONENT_TARGET} PRIVATE log_freertos.c)
else()
    target_sources(${COMPONENT_TARGET} PRIVATE log_noos.c)
endif()
