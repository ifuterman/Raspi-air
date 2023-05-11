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

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer, size_t char_offset, size_t *entry_offset_byte_rtn )
{
		int offset;
		int i;
		struct aesd_buffer_entry* entry;
		offset = 0;
		entry  = NULL;
		if(buffer->out_offs == buffer->in_offs && !buffer->full){
			return NULL;
		}
		if(buffer->out_offs < buffer->in_offs){
			for(i = buffer->out_offs; i < buffer->in_offs; i++){
				entry = &buffer->entry[i];
				if(entry->size + offset - 1 < char_offset){
					offset += entry->size;
					continue;
				}
				*entry_offset_byte_rtn = char_offset - offset;
				return entry;
			}
			return NULL;
		}
		for(i = buffer->out_offs; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
			entry = &buffer->entry[i];
			if(entry->size + offset - 1 < char_offset){
				offset += entry->size;
				continue;
			}
			*entry_offset_byte_rtn = char_offset - offset;
			return entry;
		}
		for(i = 0; i < buffer->out_offs; i++){
			entry = &buffer->entry[i];
			if(entry->size + offset - 1 < char_offset){
				offset += entry->size;
				continue;
			}
			*entry_offset_byte_rtn =char_offset - offset;
			return entry;
		}
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
	buffer->size -= buffer->entry[buffer->in_offs].size + add_entry->size;
	buffer->entry[buffer->in_offs] = *add_entry;
	
	if(buffer->in_offs == buffer->out_offs && buffer->full){
		buffer->out_offs++;
	}
	buffer->in_offs++; 
	if(buffer->in_offs < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
		return;
	}
	
	if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
		buffer->in_offs = 0;
		buffer->full = true;
	}
	if(buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
		buffer->out_offs = 0;
	}
}

long aesd_circular_buffer_get_offset_for_byte(struct aesd_circular_buffer *buffer, uint32_t entry_off, uint32_t off)
{
	uint32_t i;
	uint32_t cur_entry_offset;
	long offset;
	offset = 0;
	cur_entry_offset = 0;
	if(entry_off > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
	{
		return -1;
	}
	for(i = buffer->out_offs; i < buffer->in_offs; i++)
	{
		if(cur_entry_offset == entry_off)
		{
			if(off > buffer->entry[i].size)
			{
				return -1;
			}
			return offset + off;
		}
		cur_entry_offset++;
		offset += buffer->entry[i].size;
	}		
	if(!buffer->full)
	{
		return offset;
	}
	for(i = buffer->out_offs; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
		if(cur_entry_offset == entry_off){
			if(off > buffer->entry[i].size){
				return -1;
			}
			return offset + off;
		}
		cur_entry_offset++;
		offset += buffer->entry[i].size;
	}	
	for(i = 0; i < buffer->out_offs; i++){
		if(cur_entry_offset == entry_off){
			if(off > buffer->entry[i].size){
				return -1;
			}
			return offset + off;
		}
		cur_entry_offset++;
		offset += buffer->entry[i].size;
	}		
	return -1;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
