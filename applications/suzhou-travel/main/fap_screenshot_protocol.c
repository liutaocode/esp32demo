#include "fap_screenshot_protocol.h"

#include <string.h>

static const char REQUEST[] = "FAP_SCREENSHOT_V1";

void fap_screenshot_parser_init(fap_screenshot_parser_t *parser)
{
    parser->matched = 0;
    parser->invalid = false;
}

bool fap_screenshot_parser_feed(fap_screenshot_parser_t *parser, uint8_t byte)
{
    if (byte == '\r') {
        return false;
    }
    if (byte == '\n') {
        const bool complete = !parser->invalid &&
                              parser->matched == strlen(REQUEST);
        fap_screenshot_parser_init(parser);
        return complete;
    }
    if (!parser->invalid && parser->matched < strlen(REQUEST) &&
        byte == (uint8_t)REQUEST[parser->matched]) {
        parser->matched++;
    } else {
        parser->invalid = true;
    }
    return false;
}
