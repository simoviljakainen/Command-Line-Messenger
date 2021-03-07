#ifndef SETTING_H
    #define SETTING_H

    #include <inc/general.h>

    #define MAX_USERNAME_LEN 10
    #define MAX_PASSWORD_LEN 20
    #define MAX_MSG_LEN 256
    #define ID_SIZE 5
    #define DEFAULT_USERNAME "Heikki"
    #define DEFAULT_PASSWORD "KimmoHeikkiPena"

    #define MAX_PORT_STR 6
    #define MAX_IPV4_STR 16
    #define LOCAL_HOST "0.0.0.0"
    #define DEFAULT_PORT "1337"

    /* Argon2id */
    #define SALT_LEN 16
    #define HASH_LEN 32

    #define NANOSECS_IN_SEC 1000000000
    #define NANOSECS_IN_MICRO 1000

    typedef struct _connection{
        char ipv4[MAX_IPV4_STR];
        char port[MAX_PORT_STR];
        char password[MAX_PASSWORD_LEN];
        bool is_server;
        uint16_t fps;
        uint16_t max_connections;
    }Connection;

    typedef struct _user{
        char username[MAX_USERNAME_LEN];
    }User;

#endif

#ifdef INIT

    /* Argon2id */
    const uint32_t time_cost = 6;
	const uint32_t mem_cost = 1024; //Kibibytes
	const uint32_t threads = 1;

    Connection connection = {
        .ipv4 = LOCAL_HOST,
        .port = DEFAULT_PORT,
        .password = DEFAULT_PASSWORD,
        .is_server = false,
        .fps = 60,
        .max_connections = 2
    };

    User user = {.username = DEFAULT_USERNAME};

#else

    extern const uint32_t time_cost;
	extern const uint32_t mem_cost;
	extern const uint32_t threads;

    extern Connection connection;
    extern User user;

#endif