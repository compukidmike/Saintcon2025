#pragma once

// ---------------------------------------------------------------------------------------------
// API helper template definitions
// ---------------------------------------------------------------------------------------------

template <typename T> std::optional<T> ApiClient::ApiHelpers::json_get(const json &data, const std::string_view &path) {
    if (data.is_discarded() || data.is_null() || path.empty()) {
        return std::nullopt;
    }

    // Parse the dot-separated path
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos   = 0;

    while ((pos = path.find('.', start)) != std::string_view::npos) {
        if (pos > start) {
            parts.emplace_back(path.substr(start, pos - start));
        }
        start = pos + 1;
    }
    if (start != path.size()) {
        parts.emplace_back(path.substr(start));
    }
    if (parts.empty()) {
        return std::nullopt;
    }

    const json *current = &data;
    for (const auto &part : parts) {
        // Try as array index
        if (std::all_of(part.begin(), part.end(), ::isdigit)) {
            if (current->is_array()) {
                size_t index = std::stoull(part);
                if (index >= current->size()) {
                    return std::nullopt;
                }
                current = &(*current)[index];
                continue;
            }
        }

        // Fall back to object key
        if (!current->is_object()) {
            return std::nullopt;
        }
        auto it = current->find(part);
        if (it == current->end()) {
            return std::nullopt;
        }
        current = &it.value();
    }

    if (current->is_null()) {
        return std::nullopt;
    }
    return current->get<T>();
}

template <typename T> T ApiClient::ApiHelpers::json_get(const json &data, const std::string_view &path, const T &default_value) {
    return json_get<T>(data, path).value_or(default_value);
}

// ---------------------------------------------------------------------------------------------
// Macro to generate specializations for enum types with string conversion functions
// ---------------------------------------------------------------------------------------------
#define JSON_GET_ENUM_SPECIALIZATION(EnumType, ConversionFunc, UnknownValue)                                                   \
    template <>                                                                                                                \
    inline std::optional<EnumType> ApiClient::ApiHelpers::json_get<EnumType>(const json &data, const std::string_view &path) { \
        if (data.is_null()) {                                                                                                  \
            return (EnumType)(-1);                                                                                             \
        }                                                                                                                      \
        if (data.is_discarded() || path.empty()) {                                                                             \
            return std::nullopt;                                                                                               \
        }                                                                                                                      \
                                                                                                                               \
        /* Parse the dot-separated path */                                                                                     \
        std::vector<std::string> parts;                                                                                        \
        size_t start = 0;                                                                                                      \
        size_t pos   = 0;                                                                                                      \
                                                                                                                               \
        while ((pos = path.find('.', start)) != std::string_view::npos) {                                                      \
            if (pos > start) {                                                                                                 \
                parts.emplace_back(path.substr(start, pos - start));                                                           \
            }                                                                                                                  \
            start = pos + 1;                                                                                                   \
        }                                                                                                                      \
        if (start != path.size()) {                                                                                            \
            parts.emplace_back(path.substr(start));                                                                            \
        }                                                                                                                      \
        if (parts.empty()) {                                                                                                   \
            return std::nullopt;                                                                                               \
        }                                                                                                                      \
                                                                                                                               \
        const json *current = &data;                                                                                           \
        for (const auto &part : parts) {                                                                                       \
            /* Try as array index */                                                                                           \
            if (std::all_of(part.begin(), part.end(), ::isdigit)) {                                                            \
                if (current->is_array()) {                                                                                     \
                    size_t index = std::stoull(part);                                                                          \
                    if (index >= current->size()) {                                                                            \
                        return std::nullopt;                                                                                   \
                    }                                                                                                          \
                    current = &(*current)[index];                                                                              \
                    continue;                                                                                                  \
                }                                                                                                              \
            }                                                                                                                  \
                                                                                                                               \
            /* Fall back to object key */                                                                                      \
            if (!current->is_object()) {                                                                                       \
                return std::nullopt;                                                                                           \
            }                                                                                                                  \
            auto it = current->find(part);                                                                                     \
            if (it == current->end()) {                                                                                        \
                return std::nullopt;                                                                                           \
            }                                                                                                                  \
            current = &it.value();                                                                                             \
        }                                                                                                                      \
                                                                                                                               \
        if (current->is_null()) {                                                                                              \
            return std::nullopt;                                                                                               \
        }                                                                                                                      \
                                                                                                                               \
        /* Convert string to enum */                                                                                           \
        if (current->is_string()) {                                                                                            \
            std::string str_value = current->get<std::string>();                                                               \
            EnumType value        = ConversionFunc(str_value.c_str());                                                         \
            if (value == UnknownValue) {                                                                                       \
                return std::nullopt;                                                                                           \
            }                                                                                                                  \
            return value;                                                                                                      \
        } else if (current->is_number_integer()) {                                                                             \
            /* Also support direct integer values */                                                                           \
            return static_cast<EnumType>(current->get<int>());                                                                 \
        }                                                                                                                      \
                                                                                                                               \
        return std::nullopt;                                                                                                   \
    }

// ---------------------------------------------------------------------------------------------
// Specializations for nut/types.h enum types
// ---------------------------------------------------------------------------------------------
JSON_GET_ENUM_SPECIALIZATION(nut_type_t, get_nut_type_from_str, NUT_TYPE_UNKNOWN)
JSON_GET_ENUM_SPECIALIZATION(node_type_t, get_node_type_from_str, NODE_TYPE_UNKNOWN)
JSON_GET_ENUM_SPECIALIZATION(code_type_t, get_code_type_from_str, CODE_TYPE_UNKNOWN)

// ---------------------------------------------------------------------------------------------
// Specialization for time_t to parse ISO8601 timestamps
// ---------------------------------------------------------------------------------------------
template <> inline std::optional<time_t> ApiClient::ApiHelpers::json_get<time_t>(const json &data, const std::string_view &path) {
    if (data.is_discarded() || data.is_null() || path.empty()) {
        return std::nullopt;
    }

    // Parse the dot-separated path (same logic as generic template)
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos   = 0;

    while ((pos = path.find('.', start)) != std::string_view::npos) {
        if (pos > start) {
            parts.emplace_back(path.substr(start, pos - start));
        }
        start = pos + 1;
    }
    if (start != path.size()) {
        parts.emplace_back(path.substr(start));
    }
    if (parts.empty()) {
        return std::nullopt;
    }

    const json *current = &data;
    for (const auto &part : parts) {
        // Try as array index
        if (std::all_of(part.begin(), part.end(), ::isdigit)) {
            if (current->is_array()) {
                size_t index = std::stoull(part);
                if (index >= current->size()) {
                    return std::nullopt;
                }
                current = &(*current)[index];
                continue;
            }
        }

        // Fall back to object key
        if (!current->is_object()) {
            return std::nullopt;
        }
        auto it = current->find(part);
        if (it == current->end()) {
            return std::nullopt;
        }
        current = &it.value();
    }

    if (current->is_null()) {
        return std::nullopt;
    }

    // Parse ISO8601 timestamp string to time_t
    if (current->is_string()) {
        std::string timestamp_str = current->get<std::string>();
        return parse_iso8601(timestamp_str);
    } else if (current->is_number_integer()) {
        // Also support direct Unix timestamp integers
        return static_cast<time_t>(current->get<int64_t>());
    }

    return std::nullopt;
}
