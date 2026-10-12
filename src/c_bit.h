static inline int hex_bit_length(const char *hexStr) {
    if (!hexStr) return 0;
    if (hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) hexStr += 2;

    int count = 0;
    for (; *hexStr != '\0'; hexStr++) {
        char c = *hexStr;
        if (c == ' ' || c == '_') continue;

        int v = (c >= '0' && c <= '9') ? c - '0' :
                (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
                (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;

        if (v < 0 || count >= 32) return 0;
        count++;
    }

    return count * 4;
}

static inline int binary_bit_length(const char *binStr) {
    if (!binStr) return 0;
    if (binStr[0] == '0' && (binStr[1] == 'b' || binStr[1] == 'B')) binStr += 2;

    int count = 0;
    for (; *binStr != '\0'; binStr++) {
        char c = *binStr;
        if (c == ' ' || c == '_') continue;
        if (c != '0' && c != '1') return 0;
        if (count >= 128) return 0;
        count++;
    }

    return count;
}

static inline uint64_t hex_to_u64(const char *s) {
    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    uint64_t res = 0;
    int count = 0;
    for (; *s != '\0'; s++) {
        char c = *s;
        if (c == ' ' || c == '_') continue;

        int v = (c >= '0' && c <= '9') ? c - '0' :
                (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
                (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;

        if (v < 0 || count >= 16) return 0;

        res = (res << 4) | (uint64_t)v;
        count++;
    }

    return count > 0 ? res : 0;
}

static inline uint64_t bin_to_u64(const char *s) {
    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) s += 2;

    uint64_t res = 0;
    int count = 0;
    for (; *s != '\0'; s++) {
        char c = *s;
        if (c == ' ' || c == '_') continue;

        if (c != '0' && c != '1') return 0;
        if (count >= 64) return 0;

        res = (res << 1) | (uint64_t)(c - '0');
        count++;
    }

    return count > 0 ? res : 0;
}

static inline size_t strip_prefixed_literal(const char *s, char prefix, char *out, size_t out_size) {
    if (!out || out_size == 0) return 0;
    if (!s) { out[0] = '\0'; return 0; }
    char upper = (char)(prefix >= 'a' && prefix <= 'z' ? prefix - ('a' - 'A') : prefix);
    size_t j = 0;
    for (size_t i = 0; s[i] != '\0' && j + 1 < out_size; i++) {
        char c = s[i];
        if (c == '_' || c == ' ') continue;
        if (j == 0 && c == '0' && (s[i + 1] == prefix || s[i + 1] == upper)) { i++; continue; }
        out[j++] = c;
    }
    out[j] = '\0';
    return j;
}

static inline size_t strip_hex_literal(const char *s, char *out, size_t out_size) {
    return strip_prefixed_literal(s, 'x', out, out_size);
}

static inline size_t strip_bin_literal(const char *s, char *out, size_t out_size) {
    return strip_prefixed_literal(s, 'b', out, out_size);
}
