/*
	ctrlproxy: A modular IRC proxy
	(c) 2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _CTRLPROXY_LOCAL_H_
#define _CTRLPROXY_LOCAL_H_

/* Name for the administration channel in the Admin network */
#define ADMIN_CHANNEL "#ctrlproxy"

/* Default prefix that is assumed if not sent by the network */
#define DEFAULT_PREFIX		"(ov)@+"

/* Default channel name prefixes if not sent by the network */
#define DEFAULT_CHANTYPES 	"#&"

/* Default port that will be used if no port was specified */
#define DEFAULT_IRC_PORT "6667"

/* Standard auto-away time if auto-away is enabled but
 * no time was specified. In seconds. */
#define AUTO_AWAY_DEFAULT_TIME (10 * 60)

#endif /* _CTRLPROXY_LOCAL_H_ */
