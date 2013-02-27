/*
 * Copyright © 2012,2013  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-buffer-private.hh"


static const char *serialize_formats[] = {
  "text",
  "json",
  NULL
};

const char **
hb_buffer_serialize_list_formats (void)
{
  return serialize_formats;
}

hb_buffer_serialize_format_t
hb_buffer_serialize_format_from_string (const char *str, int len)
{
  /* Upper-case it. */
  return (hb_buffer_serialize_format_t) (hb_tag_from_string (str, len) & ~0x20202020);
}

const char *
hb_buffer_serialize_format_to_string (hb_buffer_serialize_format_t format)
{
  switch (format)
  {
    case HB_BUFFER_SERIALIZE_FORMAT_TEXT:	return serialize_formats[0];
    case HB_BUFFER_SERIALIZE_FORMAT_JSON:	return serialize_formats[1];
    default:
    case HB_BUFFER_SERIALIZE_FORMAT_INVALID:	return NULL;
  }
}

static unsigned int
_hb_buffer_serialize_glyphs_json (hb_buffer_t *buffer,
				  unsigned int start,
				  unsigned int end,
				  char *buf,
				  unsigned int buf_size,
				  unsigned int *buf_consumed,
				  hb_font_t *font,
				  hb_buffer_serialize_flags_t flags)
{
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, NULL);
  hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (buffer, NULL);

  *buf_consumed = 0;
  for (unsigned int i = start; i < end; i++)
  {
    char b[1024];
    char *p = b;

    /* In the following code, we know b is large enough that no overflow can happen. */

#define APPEND(s) HB_STMT_START { strcpy (p, s); p += strlen (s); } HB_STMT_END

    if (i)
      *p++ = ',';

    *p++ = '{';

    APPEND ("\"g\":");
    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES))
    {
      char g[128];
      hb_font_glyph_to_string (font, info[i].codepoint, g, sizeof (g));
      *p++ = '"';
      for (char *q = g; *q; q++) {
        if (*q == '"')
	  *p++ = '\\';
	*p++ = *q;
      }
      *p++ = '"';
    }
    else
      p += snprintf (p, ARRAY_LENGTH (b) - (p - b), "%u", info[i].codepoint);

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS)) {
      p += snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"cl\":%u", info[i].cluster);
    }

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS))
    {
      p += snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"dx\":%d,\"dy\":%d",
		     pos[i].x_offset, pos[i].y_offset);
      p += snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"ax\":%d,\"ay\":%d",
		     pos[i].x_advance, pos[i].y_advance);
    }

    *p++ = '}';

    if (buf_size > (p - b))
    {
      unsigned int l = p - b;
      memcpy (buf, b, l);
      buf += l;
      buf_size -= l;
      *buf_consumed += l;
      *buf = '\0';
    } else
      return i - start;
  }

  return end - start;
}

static unsigned int
_hb_buffer_serialize_glyphs_text (hb_buffer_t *buffer,
				  unsigned int start,
				  unsigned int end,
				  char *buf,
				  unsigned int buf_size,
				  unsigned int *buf_consumed,
				  hb_font_t *font,
				  hb_buffer_serialize_flags_t flags)
{
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, NULL);
  hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (buffer, NULL);
  hb_direction_t direction = hb_buffer_get_direction (buffer);

  *buf_consumed = 0;
  for (unsigned int i = start; i < end; i++)
  {
    char b[1024];
    char *p = b;

    /* In the following code, we know b is large enough that no overflow can happen. */

    if (i)
      *p++ = '|';

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES))
    {
      hb_font_glyph_to_string (font, info[i].codepoint, p, 128);
      p += strlen (p);
    }
    else
      p += snprintf (p, ARRAY_LENGTH (b) - (p - b), "%u", info[i].codepoint);

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS)) {
      p += snprintf (p, ARRAY_LENGTH (b) - (p - b), "=%u", info[i].cluster);
    }

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS))
    {
      if (pos[i].x_offset || pos[i].y_offset)
	p += snprintf (p, ARRAY_LENGTH (b) - (p - b), "@%d,%d", pos[i].x_offset, pos[i].y_offset);

      *p++ = '+';
      if (HB_DIRECTION_IS_HORIZONTAL (direction) || pos[i].x_advance)
	p += snprintf (p, ARRAY_LENGTH (b) - (p - b), "%d", pos[i].x_advance);
      if (HB_DIRECTION_IS_VERTICAL (direction) || pos->y_advance)
	p += snprintf (p, ARRAY_LENGTH (b) - (p - b), ",%d", pos[i].y_advance);
    }

    if (buf_size > (p - b))
    {
      unsigned int l = p - b;
      memcpy (buf, b, l);
      buf += l;
      buf_size -= l;
      *buf_consumed += l;
      *buf = '\0';
    } else
      return i - start;
  }

  return end - start;
}

/* Returns number of items, starting at start, that were serialized. */
unsigned int
hb_buffer_serialize_glyphs (hb_buffer_t *buffer,
			    unsigned int start,
			    unsigned int end,
			    char *buf,
			    unsigned int buf_size,
			    unsigned int *buf_consumed, /* May be NULL */
			    hb_font_t *font, /* May be NULL */
			    hb_buffer_serialize_format_t format,
			    hb_buffer_serialize_flags_t flags)
{
  assert (start <= end && end <= buffer->len);

  unsigned int sconsumed;
  if (!buf_consumed)
    buf_consumed = &sconsumed;

  *buf_consumed = 0;

  assert ((!buffer->len && buffer->content_type == HB_BUFFER_CONTENT_TYPE_INVALID) ||
	  buffer->content_type == HB_BUFFER_CONTENT_TYPE_GLYPHS);

  if (unlikely (start == end))
    return 0;

  if (!font)
    font = hb_font_get_empty ();

  switch (format)
  {
    case HB_BUFFER_SERIALIZE_FORMAT_TEXT:
      return _hb_buffer_serialize_glyphs_text (buffer, start, end,
					       buf, buf_size, buf_consumed,
					       font, flags);

    case HB_BUFFER_SERIALIZE_FORMAT_JSON:
      return _hb_buffer_serialize_glyphs_json (buffer, start, end,
					       buf, buf_size, buf_consumed,
					       font, flags);

    default:
    case HB_BUFFER_SERIALIZE_FORMAT_INVALID:
      return 0;

  }
}

hb_bool_t
hb_buffer_deserialize_glyphs (hb_buffer_t *buffer,
			      const char *buf,
			      unsigned int buf_len, /* -1 means nul-terminated */
			      unsigned int *buf_consumed, /* May be NULL */
			      hb_font_t *font, /* May be NULL */
			      hb_buffer_serialize_format_t format)
{
  return false;
}
