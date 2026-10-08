idf_component_register(
    SRCS "main.c" "cast_data.c" "cast_state.c" "cast_protocol.c" "cast_profile_online.c" "cast_sync.c" "cast_network.c" "cast_net_config.c" "cast_input.c" "cast_remote.c" "cast_remote_protocol.c" "../assets/fonts/cast_font_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp lvgl json esp_http_client esp_http_server esp_wifi esp_netif esp_event nvs_flash mbedtls esp_timer lwip spiffs
)
target_sources(${COMPONENT_LIB} PRIVATE "cast_archive.c")
