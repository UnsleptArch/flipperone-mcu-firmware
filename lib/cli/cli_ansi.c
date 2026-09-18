#include "cli_ansi.h"

typedef enum {
    CliAnsiParserStateInitial,
    CliAnsiParserStateEscape,
    CliAnsiParserStateEscapeBrace,
    CliAnsiParserStateEscapeBraceOne,
    CliAnsiParserStateEscapeBraceOneSemicolon,
    CliAnsiParserStateEscapeBraceOneSemicolonModifiers,
    // <ESC> [ 3 ..., waiting on the trailing '~' of the Delete key's
    // sequence (ESC [ 3 ~).
    CliAnsiParserStateEscapeBraceDelete,
} CliAnsiParserState;

struct CliAnsiParser {
    CliAnsiParserState state;
    CliModKey modifiers;
};

CliAnsiParser* cli_ansi_parser_alloc(void) {
    CliAnsiParser* parser = malloc(sizeof(CliAnsiParser));
    parser->state = CliAnsiParserStateInitial;
    parser->modifiers = CliModKeyNo;
    return parser;
}

void cli_ansi_parser_free(CliAnsiParser* parser) {
    free(parser);
}

/**
 * @brief Converts a single character representing a special key into the enum
 * representation
 */
static CliKey cli_ansi_key_from_mnemonic(char c) {
    switch(c) {
    case 'A':
        return CliKeyUp;
    case 'B':
        return CliKeyDown;
    case 'C':
        return CliKeyRight;
    case 'D':
        return CliKeyLeft;
    case 'F':
        return CliKeyEnd;
    case 'H':
        return CliKeyHome;
    default:
        return CliKeyUnrecognized;
    }
}

#define PARSER_RESET_AND_RETURN(parser, modifiers_val, key_val) \
    do {                                                        \
        parser->state = CliAnsiParserStateInitial;              \
        return (CliAnsiParserResult){                           \
            .is_done = true,                                    \
            .result = (CliKeyCombo){                            \
                .modifiers = modifiers_val,                     \
                .key = key_val,                                 \
            }};                                                 \
    } while(0);

CliAnsiParserResult cli_ansi_parser_feed(CliAnsiParser* parser, char c) {
    switch(parser->state) {
    case CliAnsiParserStateInitial:
        // <key> -> <key>
        if(c != CliKeyEsc) PARSER_RESET_AND_RETURN(parser, CliModKeyNo, c); // -V1048

        // <ESC> ...
        parser->state = CliAnsiParserStateEscape;
        break;

    case CliAnsiParserStateEscape:
        // <ESC> <ESC> -> <ESC>
        if(c == CliKeyEsc) PARSER_RESET_AND_RETURN(parser, CliModKeyNo, c);

        // <ESC> <key> -> Alt + <key>
        if(c != '[') PARSER_RESET_AND_RETURN(parser, CliModKeyAlt, c);

        // <ESC> [ ...
        parser->state = CliAnsiParserStateEscapeBrace;
        break;

    case CliAnsiParserStateEscapeBrace:
        // <ESC> [ 1 ...
        if(c == '1') {
            parser->state = CliAnsiParserStateEscapeBraceOne;
            break;
        }

        // <ESC> [ 3 ... (Delete key, expects a trailing '~')
        if(c == '3') {
            parser->state = CliAnsiParserStateEscapeBraceDelete;
            break;
        }

        // <ESC> [ <key mnemonic> -> <key>
        PARSER_RESET_AND_RETURN(parser, CliModKeyNo, cli_ansi_key_from_mnemonic(c));

    case CliAnsiParserStateEscapeBraceDelete:
        // <ESC> [ 3 ~ -> Delete. Consume the trailing byte either way so an
        // unexpected one doesn't leak back into the input stream as a
        // literal character the way it used to.
        if(c == '~') PARSER_RESET_AND_RETURN(parser, CliModKeyNo, CliKeyDelete);
        PARSER_RESET_AND_RETURN(parser, CliModKeyNo, CliKeyUnrecognized);

    case CliAnsiParserStateEscapeBraceOne:
        // <ESC> [ 1 <non-;> -> error
        if(c != ';') PARSER_RESET_AND_RETURN(parser, CliModKeyNo, CliKeyUnrecognized);

        // <ESC> [ 1 ; ...
        parser->state = CliAnsiParserStateEscapeBraceOneSemicolon;
        break;

    case CliAnsiParserStateEscapeBraceOneSemicolon:
        // <ESC> [ 1 ; <modifiers> ...
        parser->modifiers = (c - '0');
        parser->modifiers &= ~1;
        parser->state = CliAnsiParserStateEscapeBraceOneSemicolonModifiers;
        break;

    case CliAnsiParserStateEscapeBraceOneSemicolonModifiers:
        // <ESC> [ 1 ; <modifiers> <key mnemonic> -> <modifiers> + <key>
        PARSER_RESET_AND_RETURN(parser, parser->modifiers, cli_ansi_key_from_mnemonic(c));
    }

    return (CliAnsiParserResult){.is_done = false};
}

CliAnsiParserResult cli_ansi_parser_feed_timeout(CliAnsiParser* parser) {
    CliAnsiParserResult result = {.is_done = false};

    if(parser->state == CliAnsiParserStateEscape) {
        result.is_done = true;
        result.result.key = CliKeyEsc;
        result.result.modifiers = CliModKeyNo;
    }

    parser->state = CliAnsiParserStateInitial;
    return result;
}
