#include <rbuf.h>
#include <testing.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static int reads_count = 0;

void *worker(void *user) {
    rbuf_t *rb = (rbuf_t *)user;
    int cnt = 0;
    int retries = 0;
    for (;;) {
        if (retries >= 1000) {
            //fprintf(stderr, "No more retries!!\n");
            break;
        }
        char *v = rb_read(rb);
        if (v) {
            //printf("0x%04x - %s \n", (int)pthread_self(), v);
            __sync_fetch_and_add(&reads_count, 1);
            retries = 0;
        } else {
            pthread_testcancel();
            retries++;
            usleep(100);
        }
    }
    //printf("Worker 0x%04x leaving\n", (int)pthread_self());
    return NULL;
}

void *filler (void *user) {
    int i;
    rbuf_t *rb = (rbuf_t *)user;
    for (i = 0; i < 5000; i++) {
        char *v = malloc(40);
        sprintf(v, "test%d", i);
        rb_write(rb, v);
        //usleep(250);
    }
    //printf("Filler 0x%04x leaving\n", (int)pthread_self());
    return NULL;
}


int main(int argc, char **argv) {
    int i;

    t_init();

    int rbuf_size = 10000;

    t_testing("Create a new ringbuffer (size: %d)", rbuf_size);
    rbuf_t *rb = rb_create(rbuf_size);
    t_result(rb != NULL, "Can't create a new ringbuffer");


    t_testing("Multi-threaded producer/consumer (%d items, parallel reads/writes)", rbuf_size);
    int num_threads = 4;
    pthread_t th[num_threads];
    
    for (i = 0; i < num_threads; i++) {
        pthread_create(&th[i], NULL, worker, rb);
    }

    int num_fillers = 2;
    pthread_t filler_th[num_fillers];
    for (i = 0 ; i < num_fillers; i++) {
        pthread_create(&filler_th[i], NULL, filler, rb);
    }

    sleep(1);

    for (i = 0 ; i < num_fillers; i++) {
        pthread_join(filler_th[i], NULL);
    }

    for (i = 0 ; i < num_threads; i++) {
     //   pthread_cancel(th[i]);
        pthread_join(th[i], NULL);
    }

    t_result(rb_write_count(rb) == reads_count && reads_count == rbuf_size, "Number of reads and/or writes doesn't match the ringbuffer size "
             "reads: %d, writes: %d, size: %d", reads_count, rb_write_count(rb), rbuf_size);


    reads_count = 0;

    t_testing("Multi-threaded producer/consumer (%d items, parallel writes followed by %d parallel reads)", rbuf_size, rbuf_size);

    for (i = 0 ; i < num_fillers; i++) {
        pthread_create(&filler_th[i], NULL, filler, rb);
    }

    for (i = 0 ; i < num_fillers; i++) {
        pthread_join(filler_th[i], NULL);
    }

    for (i = 0; i < num_threads; i++) {
        pthread_create(&th[i], NULL, worker, rb);
    }


     for (i = 0 ; i < num_threads; i++) {
     //   pthread_cancel(th[i]);
        pthread_join(th[i], NULL);
    }


    t_result(rb_write_count(rb) / 2 == reads_count && reads_count == rbuf_size, "Number of reads and/or writes doesn't match the ringbuffer size "
             "reads: %d, writes: %d, size: %d", reads_count, rb_write_count(rb), rbuf_size);


}