set(indicator_sources "indicator_frame.c")
if(CONFIG_TILTMOUSE_SENSOR_INDICATOR)
    list(APPEND indicator_sources "sensor_indicator.c")
endif()
# IDF expands dependency requirements before sdkconfig is available. Keep
# these declared unconditionally; the GPIO/RMT adapter is opt-in above and
# unused symbols are removed from normal linked firmware.
idf_component_register(
    SRCS ${indicator_sources}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_rmt board
)
