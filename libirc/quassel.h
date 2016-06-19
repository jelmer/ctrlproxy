/*
	ctrlproxy: A modular IRC proxy
	(c) 2016 Jelmer Vernooij <jelmer@jelmer.uk>

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

#ifndef __QUASSEL_H__
#define __QUASSEL_H__

#define QUASSEL_MAGIC1					0x42
#define QUASSEL_MAGIC2					0xb3
#define QUASSEL_MAGIC3					0x3f
#define QUASSEL_MAGIC_OPT_SSL			0x1
#define QUASSEL_MAGIC_OPT_COMPRESSION	0x2

enum quassel_type {
	QUASSEL_TYPE_VOID = 0x00,
	QUASSEL_TYPE_BOOL = 0x01,
	QUASSEL_TYPE_UINT = 0x03,
	QUASSEL_TYPE_QVARIANT_LIST = 0x06,
	QUASSEL_TYPE_QMAP = 0x08,
	QUASSEL_TYPE_STRING = 0x0a,
	QUASSEL_TYPE_BYTESTRING = 0x0c,
};

enum quassel_proto {
	QUASSEL_PROTO_LEGACY = 0x1,
	QUASSEL_PROTO_INTERNAL = 0x0,
	QUASSEL_PROTO_DATA_STREAM = 0x2,
};

#define QUASSEL_PROTO_SENTINEL 0x80000000

#endif /* __QUASSEL_H__ */
