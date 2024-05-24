#include "philosopher.h"

// TODO: define some sem if you need
int sem_id[5];

void init() {
  // init some sem if you need
  for (int i = 0; i < 5; i++) {
        sem_id[i] = sem_open(1); // 初始值为1
    }
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
   while (1) {
      sem_p(sem_id[(id + 1) % 5]);
      sem_p(sem_id[id]);
      eat(id);
      sem_v(sem_id[id]);
      sem_v(sem_id[(id + 1) % 5]);
      think(id);
    }
}
