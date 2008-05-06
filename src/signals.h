/* Copyright (C) 2008 Sun Microsystems
 *
 * This file is part of uperf.
 *
 * uperf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * uperf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uperf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SIGNALS_H
#define	_SIGNALS_H
#include "uperf.h"

/* Error handling
 *
 * We need the ability to catch errors and do a mass exit
 * incase we have to exit. This mass exit can be triggered
 * either by a flowop error in a strand, or by a
 * control-c or something else
 *
 * The earlier model used signals to orchestrate the mass 
 * exit. A failing strand sent SIGUSR2 to the master
 * (or exited with a code of 7 incase of a process) and the
 * master shotdown the strands. There is a possiblity
 * for several race conditions in this scenario.
 *
 * The current model uses mailboxes to communicate errors
 * with the master. Before every flowop, every strand checks
 * to see if the global error flag has been tagged, and if yes
 * exits cleanly. The master waits for X seconds, and then
 * forbcibly kills the strands if they have not exited by
 * then.
 */

int master_setup_signal_handler();
int setup_strand_signal();

#endif /* _SIGNALS_H */
