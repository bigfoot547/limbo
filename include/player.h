#ifndef LIMBO_PLAYER_H_INCLUDED
#define LIMBO_PLAYER_H_INCLUDED

#include "uuid.h"

struct game_profile {
    struct uuid id;
    char name[17];

    char *textures, *texsig;
};

struct tag_client;

typedef struct {
    struct tag_client *conn;

    struct game_profile profile;
    unsigned abilities;
} player_t;

#endif // include guard
