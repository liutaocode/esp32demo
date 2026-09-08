#include "fap_screenshot_protocol.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool feed(fap_screenshot_parser_t *parser, const char *text)
{
    bool matched = false;
    for (size_t i = 0; text[i] != '\0'; i++) {
        matched = fap_screenshot_parser_feed(parser, (uint8_t)text[i]) ||
                  matched;
    }
    return matched;
}

int main(void)
{
    fap_screenshot_parser_t parser;
    fap_screenshot_parser_init(&parser);
    assert(feed(&parser, "FAP_SCREENSHOT_V1\n"));
    assert(feed(&parser, "FAP_SCREENSHOT_V1\r\n"));
    assert(!feed(&parser, "FAP_SCREENSHOT_V2\n"));
    assert(!feed(&parser, "noise FAP_SCREENSHOT_V1\n"));
    assert(!feed(&parser, "FAP_SCREEN"));
    assert(feed(&parser, "SHOT_V1\n"));
    return 0;
}
