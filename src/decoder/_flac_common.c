/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Common data structures and functions used by FLAC and OggFLAC
 */

#include "config.h"
#include "_flac_common.h"
#include "flac_metadata.h"
#include "flac_pcm.h"
#include "audio_check.h"

#include <glib.h>

#include <assert.h>

void
flac_data_init(struct flac_data *data, struct decoder * decoder,
	       struct input_stream *input_stream)
{
	pcm_buffer_init(&data->buffer);

	data->have_stream_info = false;
	data->first_frame = 0;
	data->next_frame = 0;

	data->position = 0;
	data->decoder = decoder;
	data->input_stream = input_stream;
	data->tag = NULL;
}

void
flac_data_deinit(struct flac_data *data)
{
	pcm_buffer_deinit(&data->buffer);

	if (data->tag != NULL)
		tag_free(data->tag);
}

static enum sample_format
flac_sample_format(const FLAC__StreamMetadata_StreamInfo *si)
{
	switch (si->bits_per_sample) {
	case 8:
		return SAMPLE_FORMAT_S8;

	case 16:
		return SAMPLE_FORMAT_S16;

	case 24:
		return SAMPLE_FORMAT_S24_P32;

	case 32:
		return SAMPLE_FORMAT_S32;

	default:
		return SAMPLE_FORMAT_UNDEFINED;
	}
}

bool
flac_data_get_audio_format(struct flac_data *data,
			   struct audio_format *audio_format)
{
	GError *error = NULL;

	if (!data->have_stream_info) {
		g_warning("no STREAMINFO packet found");
		return false;
	}

	data->sample_format = flac_sample_format(&data->stream_info);

	if (!audio_format_init_checked(audio_format,
				       data->stream_info.sample_rate,
				       data->sample_format,
				       data->stream_info.channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return false;
	}

	data->frame_size = audio_format_frame_size(audio_format);

	return true;
}

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     struct flac_data *data)
{
	struct replay_gain_info *rgi;

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		data->stream_info = block->data.stream_info;
		data->have_stream_info = true;
		break;

	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		rgi = flac_parse_replay_gain(block);
		if (rgi != NULL) {
			decoder_replay_gain(data->decoder, rgi);
			replay_gain_info_free(rgi);
		}

		if (data->tag != NULL)
			flac_vorbis_comments_to_tag(data->tag, NULL,
						    &block->data.vorbis_comment);

	default:
		break;
	}
}

void flac_error_common_cb(const char *plugin,
			  const FLAC__StreamDecoderErrorStatus status,
			  struct flac_data *data)
{
	if (decoder_get_command(data->decoder) == DECODE_COMMAND_STOP)
		return;

	switch (status) {
	case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
		g_warning("%s lost sync\n", plugin);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
		g_warning("bad %s header\n", plugin);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
		g_warning("%s crc mismatch\n", plugin);
		break;
	default:
		g_warning("unknown %s error\n", plugin);
	}
}

FLAC__StreamDecoderWriteStatus
flac_common_write(struct flac_data *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[],
		  FLAC__uint64 nbytes)
{
	enum decoder_command cmd;
	size_t buffer_size = frame->header.blocksize * data->frame_size;
	void *buffer;
	unsigned bit_rate;

	buffer = pcm_buffer_get(&data->buffer, buffer_size);

	flac_convert(buffer, frame->header.channels,
		     data->sample_format, buf,
		     0, frame->header.blocksize);

	if (nbytes > 0)
		bit_rate = nbytes * 8 * frame->header.sample_rate /
			(1000 * frame->header.blocksize);
	else
		bit_rate = 0;

	cmd = decoder_data(data->decoder, data->input_stream,
			   buffer, buffer_size,
			   bit_rate);
	data->next_frame += frame->header.blocksize;
	switch (cmd) {
	case DECODE_COMMAND_NONE:
	case DECODE_COMMAND_START:
		break;

	case DECODE_COMMAND_STOP:
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	case DECODE_COMMAND_SEEK:
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

char*
flac_cue_track(	const char* pathname,
		const unsigned int tnum)
{
	FLAC__bool success;
	FLAC__StreamMetadata* cs;

	success = FLAC__metadata_get_cuesheet(pathname, &cs);
	if (!success)
		return NULL;

	assert(cs != NULL);

	if (cs->data.cue_sheet.num_tracks <= 1)
	{
		FLAC__metadata_object_delete(cs);
		return NULL;
	}

	if (tnum > 0 && tnum < cs->data.cue_sheet.num_tracks)
	{
		char* track = g_strdup_printf("track_%03u.flac", tnum);

		FLAC__metadata_object_delete(cs);

		return track;
	}
	else
	{
		FLAC__metadata_object_delete(cs);
		return NULL;
	}
}

unsigned int
flac_vtrack_tnum(const char* fname)
{
	/* find last occurrence of '_' in fname
	 * which is hopefully something like track_xxx.flac
	 * another/better way would be to use tag struct
	 */
	char* ptr = strrchr(fname, '_');
	if (ptr == NULL)
		return 0;

	// copy ascii tracknumber to int
	return (unsigned int)strtol(++ptr, NULL, 10);
}

#endif /* FLAC_API_VERSION_CURRENT >= 7 */
