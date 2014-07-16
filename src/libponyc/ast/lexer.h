#ifndef LEXER_H
#define LEXER_H

#include "source.h"
#include "token.h"

typedef struct lexer_t lexer_t;


/** Create a new lexer to handle the given source.
 * The created lexer must be freed later with lexer_close().
 * Never returns NULL.
 */
lexer_t* lexer_open(source_t* source);

/** Free a previously opened lexer.
 * The lexer argument may be NULL.
 */
void lexer_close(lexer_t* lexer);

/** Get the next token from the given lexer.
 * The returned token must be freed later with token_free().
 */
token_t* lexer_next(lexer_t* lexer);

/** Report the lexical text for the given token ID.
 * The returned string should not be freed by the caller and is valid
 * indefinitely.
 * Only reports IDs with fixed text, ie operators and keywords.
 * Returns NULL for non-fixed tokens.
 */
const char* lexer_print(token_id id);

/** Report the abstract keyword that the given text represents, if any.
 * Returns the ID of the given abstract keyword or TK_LEX_ERROR if the text is
 * not an abstract keyword.
 */
token_id lexer_is_abstract_keyword(const char* text);

#endif
