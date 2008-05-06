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

#ifndef _SENDFILEV_H_
#define	_SENDFILEV_H_

int sendfile_init(char *dir);
ssize_t do_sendfilev(int sock, char *dir, int nfiles, int csize);
ssize_t do_sendfile(int sock, char *dir, int csize);

#endif /* _SENDFILEV_H_ */
