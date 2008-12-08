/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "output_control.h"
#include "output_api.h"
#include "output_internal.h"
#include "output_list.h"
#include "audio.h"

#include <glib.h>

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"

#define getBlockParam(name, str, force) { \
	bp = getBlockParam(param, name); \
	if(force && bp == NULL) { \
		g_error("couldn't find parameter \"%s\" in audio output " \
			"definition beginning at %i\n",			\
			name, param->line);				\
	} \
	if(bp) str = bp->value; \
}

int audio_output_init(struct audio_output *ao, ConfigParam * param)
{
	const char *name = NULL;
	char *format = NULL;
	BlockParam *bp = NULL;
	const struct audio_output_plugin *plugin = NULL;

	if (param) {
		const char *type = NULL;

		getBlockParam(AUDIO_OUTPUT_NAME, name, 1);
		getBlockParam(AUDIO_OUTPUT_TYPE, type, 1);
		getBlockParam(AUDIO_OUTPUT_FORMAT, format, 0);

		plugin = audio_output_plugin_get(type);
		if (plugin == NULL) {
			g_error("couldn't find audio output plugin for type "
				"\"%s\" at line %i\n", type, param->line);
		}
	} else {
		unsigned i;

		g_warning("No \"%s\" defined in config file\n",
			  CONF_AUDIO_OUTPUT);
		g_warning("Attempt to detect audio output device\n");

		audio_output_plugins_for_each(plugin, i) {
			if (plugin->test_default_device) {
				g_warning("Attempting to detect a %s audio "
					  "device\n", plugin->name);
				if (plugin->test_default_device()) {
					g_warning("Successfully detected a %s "
						  "audio device\n", plugin->name);
					break;
				}
			}
		}

		if (plugin == NULL) {
			g_warning("Unable to detect an audio device\n");
			return 0;
		}

		name = "default detected output";
	}

	ao->name = name;
	ao->plugin = plugin;
	ao->enabled = true;
	ao->open = false;
	ao->reopen_after = 0;

	ao->convBuffer = NULL;
	ao->convBufferLen = 0;

	pcm_convert_init(&ao->convState);

	if (format) {
		if (0 != parseAudioConfig(&ao->reqAudioFormat, format)) {
			g_error("error parsing format at line %i\n", bp->line);
		}
	} else
		audio_format_clear(&ao->reqAudioFormat);

	ao->thread = 0;
	notify_init(&ao->notify);
	ao->command = AO_COMMAND_NONE;

	ao->data = plugin->init(ao, format ? &ao->reqAudioFormat : NULL, param);
	if (ao->data == NULL)
		return 0;

	return 1;
}