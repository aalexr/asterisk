/* logger.h
 * Copyright © 2014, Andras Jeszenszky, JSS & Hayer IT - http://www.jsshayer.hu
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*
    conf(...)       kiírás időbélyeg nélkül, újsorral
    confn(...)      kiírás időbélyeg nélkül, újsor nélkül
    conft(...)      kiírás időbélyeggel, újsorral
    conftn(...)     kiírás időbélyeggel, újsor nélkül
*/

enum con_callmode {
    CON_CALLMODE_CONF,
    CON_CALLMODE_CONFN,
    CON_CALLMODE_CONFT,
    CON_CALLMODE_CONFTN,
    CON_CALLMODE_DEBUG,
};

#ifndef LOGGER_H_LOADED
#define LOGGER_H_LOADED
void con_init ();
//~ void conft (const char *fmt, ...);
void con_logfile (const char *file);
void con_timestamp_format (const char *format);
void _con_writef (enum con_callmode cm, char *file, int line, const char *function, const char *fmt, ...);
#endif


// ha a DEBUG makró 1, akkor a debug() makrók életbe lépnek,
// ellenkező esetben a kódba sem kerül bele :)
#ifdef CON_DEBUG
#define con_debug(...) _con_writef(CON_CALLMODE_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#else
#define con_debug(...)
#endif

#define conf(...) _con_writef(CON_CALLMODE_CONF, NULL, 0, NULL, __VA_ARGS__)
#define confn(...) _con_writef(CON_CALLMODE_CONFN, NULL, 0, NULL, __VA_ARGS__)
#define conft(...) _con_writef(CON_CALLMODE_CONFT, NULL, 0, NULL, __VA_ARGS__)
#define conftn(...) _con_writef(CON_CALLMODE_CONFTN, NULL, 0, NULL, __VA_ARGS__)
