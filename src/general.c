#include <inc/argon2.h>
#include <inc/general.h>
#include <inc/setting.h>

uint16_t str_to_uint16_t(char *string) {
  long int n;
  char *eptr;

  errno = 0;

  n = strtol(string, &eptr, 10);

  if (errno == ERANGE) {
    HANDLE_ERROR("Under or overflow with the conversion: char * -> long", 1);
  }

  if (eptr == string) {
    HANDLE_ERROR("Failed with the conversion: char * -> long", 0);
  }

  if (n > __UINT16_MAX__ || n < 0) {
    HANDLE_ERROR("Under or overflow the conversion: long -> uint16_t", 0);
  }

  return (uint16_t)n;
}

void handle_error(char *msg, int show_err, char *file, int line) {
  fprintf(
      stderr,
      "ERROR(%d) %s:%d -- %s:%s\n",
      errno,
      file, line,
      (msg != NULL) ? msg : "-",
      (show_err) ? strerror(errno) : "-");
  exit(EXIT_FAILURE);

  return;
}

/*TODO change to openssl - The seed is set in main */
uint8_t *pseudo_random_bytes(int bytes) {
  uint8_t *rand_bytes;

  if ((rand_bytes = (uint8_t *)malloc(bytes)) == NULL) {
    HANDLE_ERROR("Failed to allocate memory.", 1);
  }

  for (int i = 0; i < bytes; i++)
    rand_bytes[i] = rand() % 0x100;

  return rand_bytes;
}

char *generate_argon2id_hash(char *password) {
  uint8_t *salt = pseudo_random_bytes(SALT_LEN);

  char *argon_hash;

  size_t encoded_hash_size = argon2_encodedlen(
      time_cost, mem_cost, threads, SALT_LEN, HASH_LEN, Argon2_id);

  if ((argon_hash = (char *)malloc(encoded_hash_size)) == NULL) {
    HANDLE_ERROR("Failed to allocate memory.", 1);
  }

  argon2id_hash_encoded(
      time_cost, mem_cost,
      threads,
      password, strlen(password) + 1,  // Null byte is included
      salt, SALT_LEN,
      HASH_LEN, argon_hash, encoded_hash_size);

  free(salt);

  return argon_hash;
}

int verify_argon2id(char *hash, char *password) {
  if (argon2id_verify(hash, password, strlen(password) + 1) == ARGON2_OK)
    return 0;

  return 1;
}

struct timespec nanosec_to_timespec(long nanosecs) {
  struct timespec ts;

  ts.tv_sec = nanosecs / NANOSECS_IN_SEC;
  ts.tv_nsec = nanosecs - (ts.tv_sec * NANOSECS_IN_SEC);

  return ts;
}

long timespec_to_nanosec(struct timespec ts) {
  long nanosecs = ts.tv_sec * NANOSECS_IN_SEC;
  nanosecs += ts.tv_nsec;

  return nanosecs;
}

struct timespec remainder_timespec(struct timespec t1, struct timespec t2) {
  long nanosecs = timespec_to_nanosec(t1) - timespec_to_nanosec(t2);
  return nanosec_to_timespec(nanosecs);
}

struct timespec get_time_interval(struct timeval start, struct timeval end) {
  long time_between;

  time_between = (end.tv_sec - start.tv_sec) * NANOSECS_IN_SEC;
  time_between += (end.tv_usec - start.tv_usec) * NANOSECS_IN_MICRO;

  return nanosec_to_timespec(time_between);
}
