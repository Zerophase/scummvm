/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

/* song player structure */

#ifndef SCI_SFX_SFX_PLAYER_H
#define SCI_SFX_SFX_PLAYER_H

#include "sci/resource.h"
#include "sci/sfx/iterator.h"

#include "common/scummsys.h"

namespace Sci {

typedef void tell_synth_func(int buf_nr, byte *buf);

struct sfx_player_t {
	const char *name;
	const char *version;

	Common::Error (*set_option)(char *name, char *value);
	/* Sets an option for player timing mechanism
	** Parameters: (char *) name: The name describing what to set
	**             (char *) value: The value to set
	** Returns   : (int) Common::kNoError, or Common::kUnknownError if the name wasn't understood
	*/

	Common::Error (*init)(ResourceManager *resmgr, int expected_latency);
	/* Initializes the player
	** Parameters: (ResourceManager *) resmgr: A resource manager for driver initialization
	**             (int) expected_latency: Expected delay in between calls to 'maintenance'
	**                   (in microseconds)
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	*/

	Common::Error (*add_iterator)(SongIterator *it, uint32 start_time);
	/* Adds an iterator to the song player
	** Parameters: (songx_iterator_t *) it: The iterator to play
	**             (uint32) start_time: The time to assume as the
	**                        time the first MIDI command executes at
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	** The iterator should not be cloned (to avoid memory leaks) and
	** may be modified according to the needs of the player.
	** Implementors may use the 'sfx_iterator_combine()' function
	** to add iterators onto their already existing iterators
	*/

	Common::Error (*fade_out)();
	/* Fades out the currently playing song (within two seconds
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	*/

	Common::Error (*stop)();
	/* Stops the currently playing song and deletes the associated iterator
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	*/

	Common::Error (*iterator_message)(const SongIterator::Message &msg);
	/* Transmits a song iterator message to the active song
	** Parameters: (SongIterator::Message) msg: The message to transmit
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	** OPTIONAL -- may be NULL
	** If this method is not present, sending messages will stop
	** and re-start playing, so it is preferred that it is present
	*/

	Common::Error (*pause)(); /* OPTIONAL -- may be NULL */
	/* Pauses song playing
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	*/

	Common::Error (*resume)(); /* OPTIONAL -- may be NULL */
	/* Resumes song playing after a pause
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	*/

	Common::Error (*exit)();
	/* Stops the player
	** Returns   : (int) Common::kNoError on success, Common::kUnknownError on failure
	*/

	void (*maintenance)(); /* OPTIONAL -- may be NULL */
	/* Regularly called maintenance function
	** This function is called frequently and regularly (if present), it can be
	** used to emit sound.
	*/

	tell_synth_func *tell_synth;
	/* Pass a raw MIDI event to the synth
	Parameters: (int) argc: Length of buffer holding the midi event
	           (byte *) argv: The buffer itself
	*/

	int polyphony; /* Number of voices that can play simultaneously */

};

sfx_player_t *sfx_find_player(char *name);
/* Looks up a player by name or finds the default player
** Parameters: (char *) name: Name of the player to look up, or NULL for dedault
** Returns   : (sfx_player_t *) The player requested, or NULL if none was found
*/

tell_synth_func *sfx_get_player_tell_func();
/* Gets the callback function of the player in use.
** Returns:   (tell_synth_func *) The callback function.
*/

int sfx_get_player_polyphony();
/* Determines the polyphony of the player in use
** Returns   : (int) Number of voices the active player can emit
*/

void sfx_reset_player();
/* Tells the player to stop its internal iterator
** Parameters: None.
** Returns: Nothing.
 */

SongIterator *sfx_iterator_combine(SongIterator *it1, SongIterator *it2);
/* Combines two song iterators into one
** Parameters: (sfx_iterator_t *) it1: One of the two iterators, or NULL
**             (sfx_iterator_t *) it2: The other iterator, or NULL
** Returns   : (sfx_iterator_t *) A combined iterator
** If a combined iterator is returned, it will be flagged to be allowed to
** dispose of 'it1' and 'it2', where applicable. This means that this
** call should be used by song players, but not by the core sound system
*/

} // End of namespace Sci

#endif // SCI_SFX_SFX_PLAYER_H
