#include <wchar.h>
#include <limits.h>

#include "utils.h"
#include "protocol.h"

size_t utf8_encode_bmp(wchar_t cp, unsigned char *out) {
    if (cp < 0x0080) {
        out[0] = (unsigned char)(cp & 0x7F);
        return 1;
    } else if (cp < 0x0800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    } else {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
}

void utf_supp_to_surrogates(wchar_t in, wchar_t *high, wchar_t *low) {
    *high = 0xD800 | (in >> 10);
    *low  = 0xDC00 | (in & 0x02FF);
}

int proto_write_wlenstr(struct auto_buffer *buf, const wchar_t *str, int32_t chlen) {
    unsigned char enc[3];
    size_t encsz;
    int res = 0;

    if (chlen < 0) {
        size_t chlen2 = wcslen(str);
        if (chlen2 > INT_MAX) return -3;
        chlen = (int32_t)chlen2;
    }

    struct auto_buffer strbuf;
    ab_init(&strbuf, 0, 0);

    for (int32_t i = 0; i < chlen; ++i) {
        // TODO: Handle unrepresentable characters better
        if (str[i] > 0x10FFFF) {
            res = -4;
            goto fcomplete;
        }

        if (str[i] > 0xFFFF) {
            wchar_t high;
            wchar_t low;
            utf_supp_to_surrogates(str[i], &high, &low);

            encsz = utf8_encode_bmp(high, enc);
            if ((res = ab_push(&strbuf, enc, encsz)) < 0) goto fcomplete;
            encsz = utf8_encode_bmp(low, enc);
            if ((res = ab_push(&strbuf, enc, encsz)) < 0) goto fcomplete;
        } else {
            encsz = utf8_encode_bmp(str[i], enc);
            if ((res = ab_push(&strbuf, enc, encsz)) < 0) goto fcomplete;
        }
    }

    size_t blen = ab_getwrcur(&strbuf);

    proto_write_varint(buf, (int32_t)blen);
    res = ab_copy(buf, &strbuf, 0);

fcomplete:
    ab_free(&strbuf);
    return res;
}

void utf16be_encode_bmp(wchar_t in, unsigned char out[2]) {
    out[0] = (unsigned char)(in >> 8);
    out[1] = (unsigned char)(in & 0xFF);
}

int proto_write_utf16be_lenstr(struct auto_buffer *buf, const wchar_t *str, int32_t chlen) {
    unsigned char enc[2];
    int res = 0;

    if (chlen < 0) {
        size_t chlen2 = wcslen(str);
        if (chlen2 > INT_MAX) return -3;
        chlen = (int32_t)chlen2;
    }

    /* UTF-16 is a fixed-length encoding and the length is in characters
     * so we only need one length check (this one).
     */
    if (chlen > 0x7FFF) return -3;

    struct auto_buffer strbuf;
    ab_init(&strbuf, 0, 0);

    for (int i = 0; i < chlen; ++i) {
        if (str[i] > 0x10FFFF) {
            res = -4;
            goto fcomplete;
        }

        if (str[i] > 0xFFFF) {
            wchar_t high, low;
            utf_supp_to_surrogates(str[i], &high, &low);

            utf16be_encode_bmp(high, enc);
            if ((res = ab_push(&strbuf, enc, 2)) < 0) goto fcomplete;

            utf16be_encode_bmp(low, enc);
            if ((res = ab_push(&strbuf, enc, 2)) < 0) goto fcomplete;
        } else {
            utf16be_encode_bmp(str[i], enc);
            if ((res = ab_push(&strbuf, enc, 2)) < 0) goto fcomplete;
        }
    }

    proto_write_short(buf, (int16_t)chlen);
    res = ab_copy(buf, &strbuf, 0);

fcomplete:
    ab_free(&strbuf);
    return res;
}
