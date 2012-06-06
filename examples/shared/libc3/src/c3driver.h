/*
	c3driver.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of libc3.

	libc3 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	libc3 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with libc3.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __C3DRIVER_H___
#define __C3DRIVER_H___

#define C3_DRIVER_CALL(__o, __callback, __args...) { \
		if ((__o) && (__o)->driver) \
			for (int _di = 0; (__o)->driver[_di]; _di++) \
				if ((__o)->driver[_di]->__callback) { \
					(__o)->driver[_di]->__callback(__o, (__o)->driver[_di], ##__args); \
					break; \
				} \
	}
#define C3_DRIVER(__o, __callback, __args...) \
		C3_DRIVER_CALL(__o, __callback, ##__args)
#define C3_DRIVER_INHERITED(__o, __driver, __callback, __args...) { \
		if ((__o) && (__o)->driver) \
			for (int _di = 0; (__o)->driver[_di]; _di++) \
				if ((__o)->driver[_di] == __driver && (__o)->driver[_di+1]) { \
					(__o)->driver[_di+1]->__callback(__o, (__o)->driver[_di+1], ##__args); \
					break; \
				} \
	}
#endif /* __C3DRIVER_H___ */
