/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

static struct aesd_buffer_entry *
aesd_circular_buffer_get_entry_by_offset(struct aesd_circular_buffer *buffer,
					 size_t entry_offset)
{
	struct aesd_buffer_entry *entry = NULL;
	if ((entry_offset < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)) {
		entry = &buffer->entry[(buffer->out_offs + entry_offset) %
				       AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
	}
	return entry;
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary
 * locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing
 * the zero referenced character index if all buffer strings were concatenated
 * end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the
 * byte of the returned aesd_buffer_entry buffptr member corresponding to
 * char_offset.  This value is only set when a matching char_offset is found in
 * aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position
 * described by char_offset, or NULL if this position is not available in the
 * buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
	struct aesd_circular_buffer *buffer, size_t char_offset,
	size_t *entry_offset_byte_rtn)
{
	struct aesd_buffer_entry *returned_entry = NULL;
	struct aesd_buffer_entry *current_entry = NULL;
	uint8_t index;

	for (index = 0; index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	     index++) {
		current_entry =
			aesd_circular_buffer_get_entry_by_offset(buffer, index);
		if (current_entry->buffptr == NULL) {
			/* Reached the end of the buffer */
			break;
		} else if (char_offset < current_entry->size) {
			/* Found the entry */
			returned_entry = current_entry;
			*entry_offset_byte_rtn = char_offset;
			break;
		} else {
			/* Move to the next entry */
			char_offset -= current_entry->size;
		}
	}

	return returned_entry;
}

/**
 * @param buffer the buffer to find the end of.
 * @return the offset of the end of the buffer, or 0 if the buffer is empty.
 */
size_t aesd_circular_buffer_find_EOF_offset(struct aesd_circular_buffer *buffer)
{
	size_t eof_offset = 0;
	struct aesd_buffer_entry *current_entry = NULL;
	uint8_t index;

	for (index = 0; index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	     index++) {
		current_entry =
			aesd_circular_buffer_get_entry_by_offset(buffer, index);
		if (current_entry->buffptr == NULL) {
			/* Reached the end of the buffer */
			break;
		} else {
			/* Move to the next entry */
			eof_offset += current_entry->size;
		}
	}

	return eof_offset;
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary
 * locking must be performed by caller.
 * @param entry_offset the position to search for in the buffer list, describing
 * the offset of the entry to search for.
 * @param char_offset the position to search for in the buffer list, describing
 * the zero referenced character index if all buffer strings were concatenated
 * end to end
 * @return the absolute offset of the position described by entry_offset and
 * char_offset, or -1 if this position is not available in the buffer (not
 * enough data is written).
 */
loff_t
aesd_circular_buffer_find_absolute_offset(struct aesd_circular_buffer *buffer,
					  size_t entry_offset,
					  size_t char_offset)
{
	loff_t absolute_offset;
	struct aesd_buffer_entry *current_entry;
	uint8_t index;

	do {
		current_entry = aesd_circular_buffer_get_entry_by_offset(
			buffer, entry_offset);

		if ((current_entry == NULL) ||
		    (current_entry->buffptr == NULL) ||
		    (char_offset >= current_entry->size)) {
			absolute_offset = -1;
			break;
		}

		absolute_offset = char_offset;

		for (index = 0; index < entry_offset; index++) {
			current_entry =
				aesd_circular_buffer_get_entry_by_offset(buffer,
									 index);
			absolute_offset += current_entry->size;
		}

	} while (0);
	return absolute_offset;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in
 * buffer->in_offs. If the buffer was already full, overwrites the oldest entry
 * and advances buffer->out_offs to the new start location. Any necessary
 * locking must be handled by the caller Any memory referenced in @param
 * add_entry must be allocated by and/or must have a lifetime managed by the
 * caller.
 */
const char *
aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer,
			       const struct aesd_buffer_entry *add_entry)
{
	const char *returned_entry = NULL;

	if (buffer->full) {
		returned_entry = buffer->entry[buffer->out_offs].buffptr;
		buffer->out_offs = (buffer->out_offs + 1) %
				   AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	}

	buffer->entry[buffer->in_offs] = *add_entry;
	buffer->in_offs =
		(buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

	if (buffer->in_offs == buffer->out_offs) {
		buffer->full = true;
	}
	return returned_entry;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
	memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
