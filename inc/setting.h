#ifndef SETTING_H
    #define SETTING_H

    #define MAX_USERNAME_LEN 10
    #define MAX_MSG_LEN 256
    #define ID_SIZE 5

    /* Argon2id */
    #define SALT_LEN 16
    #define HASH_LEN 32

    #define NANOSECS_IN_SEC 1000000000
    #define NANOSECS_IN_MICRO 1000

#endif

#ifdef INIT

    /* Argon2id */
    const uint32_t time_cost = 6;
	const uint32_t mem_cost = 1024; //Kibibytes
	const uint32_t threads = 1;

    bool is_server = false;
    uint16_t target_fps = 60;
    char username[MAX_USERNAME_LEN] = "Heikki";

#else

    #include <inc/general.h>

    extern const uint32_t time_cost;
	extern const uint32_t mem_cost;
	extern const uint32_t threads;

    extern bool is_server;
    extern uint16_t target_fps;
    extern char username[MAX_USERNAME_LEN];


#endif