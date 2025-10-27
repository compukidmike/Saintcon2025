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
