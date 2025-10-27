#include "ndef.h"
#include <stdlib.h>
#include <string.h>

static int memdup_into(uint8_t **dst, const uint8_t *src, size_t len) {
    if (len == 0) {
        *dst = NULL;
        return 1;
    }
    uint8_t *p = (uint8_t *)malloc(len);
    if (!p) {
        return 0;
    }
    memcpy(p, src, len);
    *dst = p;
    return 1;
}

static int type_equals(const uint8_t *t, size_t n, const char *lit) {
    size_t m = strlen(lit);
    return (n == m) && (m == 0 || memcmp(t, lit, m) == 0);
}

ndef_status_t ndef_type5_find_ndef(const uint8_t *buf, size_t len, const uint8_t **ndef, size_t *ndef_len) {
    if (!buf || !ndef || !ndef_len) {
        return NDEF_ERR_MALFORMED;
    }
    *ndef     = NULL;
    *ndef_len = 0;
    if (len < 4 || buf[0] != 0xE1) {
        return NDEF_ERR_MALFORMED;
    }

    size_t i = 4; // TLVs after 4-byte CC
    while (i < len) {
        uint8_t t = buf[i++];
        if (t == 0x00) {
            continue; // NULL TLV
        }
        if (t == 0xFE) {
            return NDEF_ERR_NO_NDEF; // Terminator without NDEF
        }

        if (i >= len) {
            return NDEF_ERR_MALFORMED;
        }
        size_t L  = 0;
        uint8_t l = buf[i++];
        if (l == 0xFF) {
            if (i + 2 > len) {
                return NDEF_ERR_MALFORMED;
            }
            L = ((size_t)buf[i] << 8) | buf[i + 1];
            i += 2;
        } else {
            L = l;
        }
        if (i + L > len) {
            L = (len > i) ? (len - i) : 0;
        }

        if (t == 0x03) { // NDEF Message TLV
            *ndef     = &buf[i];
            *ndef_len = L;
            return (*ndef_len > 0) ? NDEF_OK : NDEF_ERR_NO_NDEF;
        }
        i += L;
    }
    return NDEF_ERR_NO_NDEF;
}

ndef_status_t ndef_parse_message(const uint8_t *bytes, size_t len, ndef_message_t *out) {
    if (!bytes || !out) {
        return NDEF_ERR_MALFORMED;
    }
    out->records = NULL;
    out->count   = 0;

    // Two pass: count records, then allocate and fill
    size_t i = 0, count = 0;

    // pass 1: count
    while (i < len) {
        if (i + 2 > len) {
            return (count ? NDEF_ERR_MALFORMED : NDEF_ERR_MALFORMED);
        }
        uint8_t hdr      = bytes[i++];
        uint8_t type_len = bytes[i++];
        int sr           = (hdr & 0x10) != 0;
        int il           = (hdr & 0x08) != 0;
        int cf           = (hdr & 0x20) != 0;
        if (cf) {
            return NDEF_ERR_MALFORMED; // chunking not supported
        }

        size_t payload_len = 0;
        if (sr) {
            if (i + 1 > len) {
                return NDEF_ERR_MALFORMED;
            }
            payload_len = bytes[i++];
        } else {
            if (i + 4 > len) {
                return NDEF_ERR_MALFORMED;
            }
            payload_len =
                ((size_t)bytes[i] << 24) | ((size_t)bytes[i + 1] << 16) | ((size_t)bytes[i + 2] << 8) | (size_t)bytes[i + 3];
            i += 4;
        }

        size_t id_len = 0;
        if (il) {
            if (i + 1 > len) {
                return NDEF_ERR_MALFORMED;
            }
            id_len = bytes[i++];
        }

        if (i + type_len > len) {
            return NDEF_ERR_MALFORMED;
        }
        i += type_len;
        if (i + id_len > len) {
            return NDEF_ERR_MALFORMED;
        }
        i += id_len;
        if (i + payload_len > len) {
            return NDEF_ERR_MALFORMED;
        }
        i += payload_len;

        count++;
        if (hdr & 0x40) {
            break; // ME
        }
    }
    if (count == 0) {
        return NDEF_ERR_MALFORMED;
    }

    // pass 2: fill
    ndef_record_t *recs = (ndef_record_t *)calloc(count, sizeof(*recs));
    if (!recs) {
        return NDEF_ERR_NOMEM;
    }

    i          = 0;
    size_t idx = 0;
    while (idx < count) {
        uint8_t hdr      = bytes[i++];
        uint8_t type_len = bytes[i++];
        int sr           = (hdr & 0x10) != 0;
        int il           = (hdr & 0x08) != 0;

        size_t payload_len = 0;
        if (sr) {
            payload_len = bytes[i++];
        } else {
            payload_len =
                ((size_t)bytes[i] << 24) | ((size_t)bytes[i + 1] << 16) | ((size_t)bytes[i + 2] << 8) | (size_t)bytes[i + 3];
            i += 4;
        }

        size_t id_len = 0;
        if (il) {
            id_len = bytes[i++];
        }

        const uint8_t *type_ptr = &bytes[i];
        i += type_len;
        const uint8_t *id_ptr = (il ? &bytes[i] : NULL);
        i += id_len;
        const uint8_t *pl_ptr = &bytes[i];
        i += payload_len;

        recs[idx].tnf = (ndef_tnf_t)(hdr & 0x07);
        if (!memdup_into(&recs[idx].type, type_ptr, type_len) || !memdup_into(&recs[idx].id, id_ptr, id_len) ||
            !memdup_into(&recs[idx].payload, pl_ptr, payload_len)) {
            // free partially built
            for (size_t k = 0; k <= idx; ++k) {
                free(recs[k].type);
                free(recs[k].id);
                free(recs[k].payload);
            }
            free(recs);
            return NDEF_ERR_NOMEM;
        }
        recs[idx].type_len    = type_len;
        recs[idx].id_len      = id_len;
        recs[idx].payload_len = payload_len;

        idx++;
    }

    out->records = recs;
    out->count   = count;
    return NDEF_OK;
}

void ndef_free_message(ndef_message_t *msg) {
    if (!msg || !msg->records) {
        return;
    }
    for (size_t i = 0; i < msg->count; ++i) {
        free(msg->records[i].type);
        free(msg->records[i].id);
        free(msg->records[i].payload);
    }
    free(msg->records);
    msg->records = NULL;
    msg->count   = 0;
}

int ndef_decode_text(const ndef_record_t *rec, char **out) {
    if (!rec || !out) {
        return 0;
    }
    *out = NULL;
    if (rec->tnf != NDEF_TNF_WELL_KNOWN) {
        return 0;
    }
    if (!type_equals(rec->type, rec->type_len, "T")) {
        return 0;
    }
    if (rec->payload_len < 1) {
        return 0;
    }

    const uint8_t *p = rec->payload;
    uint8_t status   = p[0];
    size_t lang_len  = (size_t)(status & 0x3F);
    int utf16        = (status & 0x80) != 0;
    if (utf16) {
        return 0; // not supported here
    }
    if (1 + lang_len > rec->payload_len) {
        return 0;
    }
    size_t text_len = rec->payload_len - 1 - lang_len;

    char *s = (char *)malloc(text_len + 1);
    if (!s) {
        return 0;
    }
    memcpy(s, p + 1 + lang_len, text_len);
    s[text_len] = '\0';
    *out        = s;
    return 1;
}

int ndef_decode_uri(const ndef_record_t *rec, char **out) {
    // clang-format off
    static const char *uic[] = {
        "", "http://www.", "https://www.", "http://", "https://", "tel:", "mailto:",
        "ftp://anonymous:anonymous@", "ftp://ftp.", "ftps://", "sftp://", "smb://",
        "nfs://", "ftp://", "dav://", "news:", "telnet://", "imap:", "rtsp://",
        "urn:", "pop:", "sip:", "sips:", "tftp:", "btspp://", "btl2cap://",
        "btgoep://", "tcpobex://", "irdaobex://", "file://", "urn:epc:id:",
        "urn:epc:tag:", "urn:epc:pat:", "urn:epc:raw:", "urn:epc:", "urn:nfc:"
    };
    // clang-format on
    if (!rec || !out) {
        return 0;
    }
    *out = NULL;
    if (rec->tnf != NDEF_TNF_WELL_KNOWN) {
        return 0;
    }
    if (!type_equals(rec->type, rec->type_len, "U")) {
        return 0;
    }
    if (rec->payload_len < 1) {
        return 0;
    }

    const uint8_t *p   = rec->payload;
    uint8_t code       = p[0];
    const char *prefix = "";
    if (code < (uint8_t)(sizeof(uic) / sizeof(uic[0]))) {
        prefix = uic[code];
    }

    size_t tail_len   = rec->payload_len - 1;
    size_t prefix_len = strlen(prefix);
    char *s           = (char *)malloc(prefix_len + tail_len + 1);
    if (!s) {
        return 0;
    }

    memcpy(s, prefix, prefix_len);
    memcpy(s + prefix_len, p + 1, tail_len);
    s[prefix_len + tail_len] = '\0';
    *out                     = s;
    return 1;
}

size_t ndef_type5_build_empty(uint8_t *dst, size_t dst_len) {
    // CC: E1 40 40 05, TLV: 03 00, Terminator: FE
    static const uint8_t kEmpty[7] = {0xE1, 0x40, 0x40, 0x05, 0x03, 0x00, 0xFE};
    if (dst && dst_len >= sizeof(kEmpty)) {
        memcpy(dst, kEmpty, sizeof(kEmpty));
    }
    return sizeof(kEmpty);
}