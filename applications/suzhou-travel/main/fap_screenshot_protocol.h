#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t matched;
    bool invalid;
} fap_screenshot_parser_t;

void fap_screenshot_parser_init(fap_screenshot_parser_t *parser);
bool fap_screenshot_parser_feed(fap_screenshot_parser_t *parser, uint8_t byte);
