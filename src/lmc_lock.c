#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "lmc_lock.h"

int c_l(lmc_lock_t *l, lmc_error_t *e) { 
  if (!l) { 
    lmc_handle_error_with_err_string("check_lock",
        "Semaphore not initialized", "LocalMemCacheError", e);
  }
  return l != NULL; 
}

lmc_lock_t *lmc_lock_init(const char *namespace, int init, lmc_error_t *e) {
  lmc_lock_t *l = malloc(sizeof(lmc_lock_t));
  if (!l) return NULL;
  strncpy((char *)&l->namespace, namespace, 1023);
  
  lmc_handle_error((l->sem = sem_open(l->namespace, O_CREAT, 0600, init)) == NULL,
      "sem_open", "LockError", e);
  if (!l->sem) { free(l); return NULL; }
  return l;
}

int lmc_clear_namespace_lock(const char *namespace) {
  lmc_error_t e;
  lmc_lock_t *l = lmc_lock_init(namespace, 1, &e);
  //printf("clear_namespace locks: %s %d\n", namespace, lmc_lock_get_value(l));
  lmc_lock_repair(l);
  //printf("AFTER clear_namespace locks: %s %d\n", namespace, 
      //lmc_lock_get_value(l));
  free(l);
  return 1;
}

int lmc_is_locked(lmc_lock_t* l, lmc_error_t *e) {
  if (!c_l(l, e)) { return 0; }
  if (sem_trywait(l->sem) == -1) {
    return 1;
  } else {
    sem_post(l->sem);
    return 0;
  }
}

int lmc_sem_timed_wait(lmc_lock_t* l) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
#ifdef DO_TEST_CRASH
  ts.tv_sec += 1;
#else
  ts.tv_sec += 2;
#endif
  return sem_timedwait(l->sem, &ts);
}

int lmc_sem_timed_wait_mandatory(lmc_lock_t* l) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 20;
  return sem_timedwait(l->sem, &ts);
}

int lmc_is_lock_working(lmc_lock_t* l, lmc_error_t *e) {
  if (!c_l(l, e)) { return 0; }
  if (lmc_sem_timed_wait(l) == -1) {
    return 0;
  } else {
    sem_post(l->sem);
    return 1;
  }
}

void lmc_lock_repair(lmc_lock_t *l) {
  int v; 
  sem_getvalue(l->sem, &v);
  if (v == 0) { 
    sem_post(l->sem); 
  }
  sem_getvalue(l->sem, &v);
  while (v > 1) { 
    sem_wait(l->sem); 
    sem_getvalue(l->sem, &v);
  }
}

int lmc_lock_get_value(lmc_lock_t* l) {
  int v = 0;
  sem_getvalue(l->sem, &v);
  return v;
}

int lmc_lock_obtain(const char *where, lmc_lock_t* l, lmc_error_t *e) {
  if (!c_l(l,e)) { return 0; }
  int r = lmc_sem_timed_wait(l);
  if (r == -1 && errno == ETIMEDOUT) {
    lmc_handle_error_with_err_string("sem_timedwait", strerror(errno), 
        "LockTimedOut", e);
    return 0;
  }
  return lmc_handle_error(r, "sem_timedwait", "LockError", e);
}

int lmc_lock_obtain_mandatory(const char *where, lmc_lock_t* l, lmc_error_t *e) {
  int r = lmc_sem_timed_wait_mandatory(l);
  if (r == -1 && errno == ETIMEDOUT) {
    lmc_handle_error_with_err_string("sem_timedwait", strerror(errno), 
        "LockTimedOut", e);
    return 0;
  }
  return c_l(l,e) && lmc_handle_error(r, "sem_wait", "LockError", e);
}

int lmc_lock_release(const char *where, lmc_lock_t* l, lmc_error_t *e) {
  return c_l(l, e) && lmc_handle_error(sem_post(l->sem) == -1, "sem_post", 
      "LockError", e);
}
