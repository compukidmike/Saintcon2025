idf_component_register(SRCS "api_client.cpp" "api.cpp" "types.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES "app_update" "esp-boost" "esp_http_client" "esp_https_ota" "esp_wifi" "nlohmann-json" "secure_element" "nut" "nvs"
                       EMBED_TXTFILES "certs/isrgrootx1.pem")
