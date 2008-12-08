/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

/*
 * Support library for the "idle" command.
 *
 */

#ifndef MPD_IDLE_H
#define MPD_IDLE_H

enum {
	/** song database has been updated*/
	IDLE_DATABASE = 0x1,

	/** a stored playlist has been modified, created, deleted or
	    renamed */
	IDLE_STORED_PLAYLIST = 0x2,

	/** the current playlist has been modified */
	IDLE_PLAYLIST = 0x4,

	/** the player state has changed: play, stop, pause, seek, ... */
	IDLE_PLAYER = 0x8,

	/** the volume has been modified */
	IDLE_MIXER = 0x10,

	/** an audio output device has been enabled or disabled */
	IDLE_OUTPUT = 0x20,

	/** options have changed: crossfade, random, repeat, ... */
	IDLE_OPTIONS = 0x40,
};

/**
 * Adds idle flag (with bitwise "or") and queues notifications to all
 * clients.
 */
void
idle_add(unsigned flags);

/**
 * Atomically reads and resets the global idle flags value.
 */
unsigned
idle_get(void);

/**
 * Get idle names
 */
const char*const*
idle_get_names(void);

#endif