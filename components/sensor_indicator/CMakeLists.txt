set(indicator_sources "indicator_frame.c")
set(indicator_requires "")
if(CONFIG_TILTMOUSE_SENSOR_INDICATOR)
    list(APPEND indicator_sources "sensor_indicator.c")
    list(APPEND indicator_requires esp_driver_rmt board)
endif()
idf_component_register(
    SRCS ${indicator_sources}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${indicator_requires}
)
