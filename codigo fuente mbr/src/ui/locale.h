#ifndef VOS_LOCALE_H
#define VOS_LOCALE_H

#include <stdint.h>

enum vos_language {
    VOS_LANGUAGE_SPANISH = 0,
    VOS_LANGUAGE_ENGLISH = 1,
    VOS_LANGUAGE_COUNT
};

void locale_init(void);
enum vos_language locale_current(void);
int locale_set(enum vos_language language);
void locale_toggle(void);
const char *locale_name(void);
const char *locale_translate(const char *spanish_text);

#endif
