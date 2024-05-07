/* debug.h
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#define debi(x) printf(#x " = %d\n", ((int)(x)))
#define debf(x) printf(#x " = %f\n", (x))
#define debs(x) printf(#x " = %s\n", (x))

