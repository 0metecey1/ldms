
#ifndef _TLC5948A_H_
#define _TLC5948A_H_
//  version macros for compile-time API detection

#define TLC5948A_VERSION_MAJOR 3
#define TLC5948A_VERSION_MINOR 0
#define TLC5948A_VERSION_PATCH 0

#define TLC5948A_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define TLC5948A_VERSION \
    TLC5948A_MAKE_VERSION(TLC5948A_VERSION_MAJOR, TLC5948A_VERSION_MINOR, TLC5948A_VERSION_PATCH)
#include <lua.h>
#define STATUS_LED_BLUE  0
#define STATUS_LED_GREEN 1
#define STATUS_LED_RED   2
#define PORT_LED_BLUE    3
#define PORT_LED_GREEN   4
#define PORT_LED_RED     5

//  Opaque class structures to allow forward references
typedef struct _tlc5948a_t tlc5948a_t;

tlc5948a_t * tlc5948a_create(const char *tlc5948a_path);
void tlc5948a_destroy(tlc5948a_t **self_p);
void tlc5948a_set_brightness(tlc5948a_t *self, unsigned int ch, unsigned int level);
void tlc5948a_turn_on(tlc5948a_t *self, unsigned int ch);
void tlc5948a_turn_off(tlc5948a_t *self, unsigned int ch);
void tlc5948a_turn_all_off(tlc5948a_t *self);
int tlc5948a_is_on(tlc5948a_t *self, unsigned int ch);
int luaopen_tlc5948a(lua_State *L);
#endif
