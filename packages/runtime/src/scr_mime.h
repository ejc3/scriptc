/* scriptc runtime — util.MIMEType and util.MIMEParams (scr_mime.c).
 * Included by scr_runtime.h, which says where new declarations go. */
#ifndef SCR_MIME_H
#define SCR_MIME_H

#include "scr_dyn.h"

/* ── util.MIMEType / util.MIMEParams (scr_mime.c) ───────────────────────
 * Node 24's lib/internal/mime.js as native checked-dynamic handles: the
 * getters, setters and methods dispatch through the handle. The
 * constructor takes its argument vector as one dyn array (borrowed) and
 * converts the first value with ToString; an invalid input throws Node's
 * ERR_INVALID_MIME_SYNTAX TypeError (NULL with the exception pending).
 * parse answers null instead of throwing. scr_mime_is answers
 * `v instanceof MIMEType` (kind 0) or `MIMEParams` (kind 1). */
ScrDyn *scr_mime_type_new(ScrDyn *args);
ScrDyn *scr_mime_type_parse(ScrDyn *input);
ScrDyn *scr_mime_params_new(void);
bool scr_mime_is(const ScrDyn *value, double kind);

#endif /* SCR_MIME_H */
