#include "ramfs.h"

static struct ramfs_file files[RAMFS_MAX_FILES];
static uint32_t file_count;

static uint32_t copy_text(char *destination, uint32_t capacity,
                          const char *source) {
    uint32_t length = 0;
    if (capacity == 0) return 0;
    while (source[length] != '\0' && length + 1 < capacity) {
        destination[length] = source[length];
        length++;
    }
    destination[length] = '\0';
    return length;
}

void ramfs_init(void) {
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; i++) {
        for (uint32_t j = 0; j < RAMFS_NAME_CAPACITY; j++) {
            files[i].name[j] = '\0';
        }
        for (uint32_t j = 0; j < RAMFS_CONTENT_CAPACITY; j++) {
            files[i].content[j] = '\0';
        }
        files[i].length = 0;
    }
    file_count = 1;
    (void)copy_text(files[0].name, sizeof(files[0].name), "LEEME.TXT");
    files[0].length = (uint16_t)copy_text(
        files[0].content, sizeof(files[0].content),
        "VOS guarda estos archivos en memoria RAM temporal.\n"
        "No accede al disco de Windows.\n"
        "Las notas se borran al apagar VOS.");
}

uint32_t ramfs_count(void) {
    return file_count;
}

uint32_t ramfs_capacity(void) {
    return RAMFS_MAX_FILES;
}

const struct ramfs_file *ramfs_get(uint32_t index) {
    return index < file_count ? &files[index] : 0;
}

int ramfs_create_note(void) {
    if (file_count >= RAMFS_MAX_FILES) return -1;

    struct ramfs_file *file = &files[file_count];
    file->name[0] = 'N';
    file->name[1] = 'O';
    file->name[2] = 'T';
    file->name[3] = 'A';
    uint32_t number = file_count;
    uint32_t digits[3];
    uint32_t digit_count = 0;
    do {
        digits[digit_count++] = number % 10;
        number /= 10;
    } while (number != 0 && digit_count < 3);

    uint32_t name_index = 4;
    while (digit_count != 0) {
        file->name[name_index++] = (char)('0' + digits[--digit_count]);
    }
    file->name[name_index++] = '.';
    file->name[name_index++] = 'T';
    file->name[name_index++] = 'X';
    file->name[name_index++] = 'T';
    file->name[name_index] = '\0';
    file->content[0] = '\0';
    file->length = 0;
    return (int)file_count++;
}

int ramfs_append(uint32_t index, uint32_t codepoint) {
    if (index >= file_count || codepoint == 0 ||
        (codepoint != '\n' && (codepoint < 0x20 || codepoint > 0x10ffff ||
         (codepoint >= 0x7f && codepoint <= 0x9f) ||
         (codepoint >= 0xd800 && codepoint <= 0xdfff)))) {
        return 0;
    }

    uint8_t encoded[4];
    uint32_t encoded_length;
    if (codepoint <= 0x7f) {
        encoded[0] = (uint8_t)codepoint;
        encoded_length = 1;
    } else if (codepoint <= 0x7ff) {
        encoded[0] = (uint8_t)(0xc0 | (codepoint >> 6));
        encoded[1] = (uint8_t)(0x80 | (codepoint & 0x3f));
        encoded_length = 2;
    } else if (codepoint <= 0xffff) {
        encoded[0] = (uint8_t)(0xe0 | (codepoint >> 12));
        encoded[1] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[2] = (uint8_t)(0x80 | (codepoint & 0x3f));
        encoded_length = 3;
    } else {
        encoded[0] = (uint8_t)(0xf0 | (codepoint >> 18));
        encoded[1] = (uint8_t)(0x80 | ((codepoint >> 12) & 0x3f));
        encoded[2] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3f));
        encoded[3] = (uint8_t)(0x80 | (codepoint & 0x3f));
        encoded_length = 4;
    }

    struct ramfs_file *file = &files[index];
    uint32_t available = RAMFS_CONTENT_CAPACITY - 1u - file->length;
    if (encoded_length > available) return 0;
    for (uint32_t i = 0; i < encoded_length; i++) {
        file->content[file->length++] = (char)encoded[i];
    }
    file->content[file->length] = '\0';
    return 1;
}

int ramfs_backspace(uint32_t index) {
    if (index >= file_count || files[index].length == 0) return 0;
    struct ramfs_file *file = &files[index];
    uint32_t start = file->length - 1;
    while (start > 0 &&
           (((uint8_t)file->content[start] & 0xc0u) == 0x80u)) {
        start--;
    }
    file->length = (uint16_t)start;
    file->content[start] = '\0';
    return 1;
}
