#ifndef LIMBO_UTF_H_INCLUDED
#define LIMBO_UTF_H_INCLUDED

int proto_write_wlenstr(struct auto_buffer *buf, const wchar_t *str, int32_t chlen);
int proto_write_utf16be_lenstr(struct auto_buffer *buf, const wchar_t *str, int32_t chlen);

#endif // include guard
