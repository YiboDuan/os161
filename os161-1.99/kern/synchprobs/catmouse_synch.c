#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>

/* 
 * This simple default synchronization mechanism allows only creature at a time to
 * eat.   The globalCatMouseSem is used as a a lock.   We use a semaphore
 * rather than a lock so that this code will work even before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock **bowl_locks;
static struct cv *mice_not_eating;
static struct cv *cats_not_eating;
static struct semaphore *cat_mutex;
static struct semaphore *mice_mutex;
static volatile bool whos_eating;    // false = cats, true = mice
static struct semaphore *whos_mutex;
static struct semaphore *cat_mutex_w;
static struct semaphore *mice_mutex_w;

static volatile int num_cats_eating;
static volatile int num_mice_eating;
static volatile int num_cats_waiting;
static volatile int num_mice_waiting;


/* 
 * The CatMouse simulation will call this function once before any cat or
 * mouse tries to each.
 *
 * You can use it to initialize synchronization and other variables.
 * 
 * parameters: the number of bowls
 */
void
catmouse_sync_init(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_init */
    bowl_locks = kmalloc(bowls*sizeof(struct lock *));
    for (int i = 0; i < bowls; i++) {
        kprintf("initializing bowl lock %d\n", i);
        bowl_locks[i] = lock_create("bowl lock");
    }
    
    num_cats_eating = 0;
    num_mice_eating = 0;
    num_cats_waiting = 0;
    num_mice_waiting = 0;
    
    mice_not_eating = cv_create("mice not eating");
    cats_not_eating = cv_create("cats not eating");
    
    cat_mutex = sem_create("cat mutex", 1);
    mice_mutex = sem_create("mice mutex", 1);
    whos_mutex = sem_create("whos mutex", 1);
    cat_mutex_w = sem_create("cat mutex w", 1);
    mice_mutex_w = sem_create("mice mutex w", 1);
    whos_eating = false;
    return;
}

/* 
 * The CatMouse simulation will call this function once after all cat
 * and mouse simulations are finished.
 *
 * You can use it to clean up any synchronization and other variables.
 *
 * parameters: the number of bowls
 */
void
catmouse_sync_cleanup(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_cleanup */
    for (int i = 0; i < bowls; i++) {
        lock_destroy(bowl_locks[i]);
    }
    kfree(bowl_locks);
    
    cv_destroy(mice_not_eating);
    cv_destroy(cats_not_eating);
    
    sem_destroy(cat_mutex);
    sem_destroy(mice_mutex);
}


/*
 * The CatMouse simulation will call this function each time a cat wants
 * to eat, before it eats.
 * This function should cause the calling thread (a cat simulation thread)
 * to block until it is OK for a cat to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the cat is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_before_eating(unsigned int bowl) 
{
    kprintf("cat before eating in bowl %d\n", bowl);
    KASSERT(bowl_locks[bowl-1] != NULL);
    kprintf("cat acquiring lock for bowl %d\n", bowl);
    P(cat_mutex_w);
    num_cats_waiting++;
    V(cat_mutex_w);
    lock_acquire(bowl_locks[bowl-1]);
    kprintf("cat lock acquired for bowl %d\n", bowl);
    while(whos_eating == true || num_mice_eating > 0) {
        kprintf("mice are eating... gotta wait\n");
        cv_wait(mice_not_eating, bowl_locks[bowl-1]);
    }
    P(cat_mutex_w);
    num_cats_waiting--;
    V(cat_mutex_w);
    P(cat_mutex);
    num_cats_eating++;
    V(cat_mutex);
    kprintf("cat is going to eat at bowl %d\n", bowl);
    kprintf("now there are %d cats eating, and %d mice eating\n", num_cats_eating, num_mice_eating);
}

/*
 * The CatMouse simulation will call this function each time a cat finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this cat finished.
 *
 * parameter: the number of the bowl at which the cat is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_after_eating(unsigned int bowl) 
{
    kprintf("cat finished eating at bowl %d\n", bowl);
    KASSERT(bowl_locks[bowl-1] != NULL);
    P(cat_mutex);
    num_cats_eating--;
    V(cat_mutex);
    lock_release(bowl_locks[bowl-1]);
    if(num_cats_eating == 0 && num_cats_waiting == 0 && num_mice_waiting > 0) {
        P(whos_mutex);
        whos_eating = true;
        V(whos_mutex);
        kprintf("who's eating changed to %d\n", whos_eating);
        kprintf("last cat finished at bowl %d, waking mice\n", bowl);
        cv_broadcast(cats_not_eating, bowl_locks[bowl-1]);
    }
    
}

/*
 * The CatMouse simulation will call this function each time a mouse wants
 * to eat, before it eats.
 * This function should cause the calling thread (a mouse simulation thread)
 * to block until it is OK for a mouse to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the mouse is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_before_eating(unsigned int bowl) 
{
    kprintf("mouse before eating in bowl %d\n", bowl);
    KASSERT(bowl_locks[bowl-1] != NULL);
    P(mice_mutex_w);
    num_mice_waiting++;
    V(mice_mutex_w);
    kprintf("mouse acquiring lock for bowl %d\n", bowl);
    lock_acquire(bowl_locks[bowl-1]);
    kprintf("mouse lock acquired for bowl %d\n", bowl);
    if(num_cats_eating == 0 && num_cats_waiting == 0) {
        P(whos_mutex);
        whos_eating = true;
        V(whos_mutex);
    }

    while((whos_eating == false || num_cats_eating > 0)) {
        kprintf("cats are eating... gotta wait\n");
        cv_wait(cats_not_eating, bowl_locks[bowl-1]);
    }
    P(mice_mutex_w);
    num_mice_waiting--;
    V(mice_mutex_w);
    P(mice_mutex);
    num_mice_eating++;
    V(mice_mutex);
    
    kprintf("mouse is going to eat at bowl %d\n", bowl);
    kprintf("now there are %d cats eating, and %d mice eating\n", num_cats_eating, num_mice_eating);
}

/*
 * The CatMouse simulation will call this function each time a mouse finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this mouse finished.
 *
 * parameter: the number of the bowl at which the mouse is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_after_eating(unsigned int bowl) 
{
    kprintf("mouse finished eating at bowl %d\n", bowl);
    KASSERT(bowl_locks[bowl-1] != NULL);
    
    P(mice_mutex);
    num_mice_eating--;
    V(mice_mutex);
    lock_release(bowl_locks[bowl-1]);
    if(num_mice_eating == 0 && num_mice_waiting == 0 && num_cats_waiting > 0) {
        P(whos_mutex);
        whos_eating = false;
        V(whos_mutex);
        kprintf("who's eating changed to %d\n", whos_eating);
        kprintf("last mouse finished at bowl %d, waking cats\n", bowl);
        cv_broadcast(mice_not_eating, bowl_locks[bowl-1]);
    }
    
}
