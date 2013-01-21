/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


void lock_priority_propagate (struct lock * UNUSED, struct thread * UNUSED);
bool lock_priority_cmp(const struct list_elem *,const struct list_elem *,void * UNUSED);
bool cond_priority_cmp(const struct list_elem *,const struct list_elem *,void * UNUSED);


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  struct thread *t = thread_current();

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_insert_ordered(&sema->waiters,&t->elem,&thread_priority_cmp,NULL);
      thread_set_blocking_object(t,sema,sema_type);
      thread_block ();
    }
  sema->value--;
  thread_set_blocking_object(t,NULL,none);
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) 
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  sema->value++;
  intr_set_level (old_level);

  thread_yield();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  lock->priority = -1;
  lock->elem.prev = NULL;
  lock->elem.next = NULL;
  list_init (&lock->waiters);
}


bool 
lock_priority_cmp(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED) 
{
  struct lock *la = list_entry(a,struct lock, elem);
  struct lock *lb = list_entry(b,struct lock, elem);

  return la->priority > lb->priority;
}

/* Propagate priority from blocking lock to holding thread recursively */
void
lock_priority_propagate (struct lock *lock, struct thread *t)
{

  while (true) {
    
    if (t->priority <= lock->priority) break;    
    list_move_front(&lock->waiters,&t->elem);
    lock->priority = t->priority;
    t = lock->holder;
      
    ASSERT (t != NULL);

    /* maintain invariant that lock with the highest priority is 
      placed at front of list */
    list_move_front(&t->locks_held,&lock->elem);

    if (lock->priority <= t->priority)  break;
    t->priority = lock->priority;
    t->has_donation = true;

    blocking_object_type btype;
    void * bobj = thread_get_blocking_object(t,&btype);
    
    struct semaphore *sema;
    switch(btype) {
      case none: /* Reached a non-blocked thread */
        return;
      case sema_type: /* Current thread blocked by a semaphore */
        sema = (struct semaphore *) bobj;
        list_move_front(&sema->waiters,&t->elem);
        return; 
      case lock_type: /* Current thread blocked by a lock */
        lock = (struct lock *) bobj;
        break;
      case cond_type:
        return;
    }
  }
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level;

  struct thread *t = thread_current ();

  /* Attempt to acquire lock */
  old_level = intr_disable();

  while (lock->holder != NULL) {
    thread_set_blocking_object(t,lock,lock_type);
    lock_priority_propagate(lock,t);
    thread_block ();
  }
  lock->holder = t;

  /* No need to propogate priority from lock to thread
     because running thread by definition has highest priority */

  /* Lock is acquired by t and thread t is now running */
  thread_set_blocking_object(t,NULL,none);

  /* maintain invariant that lock with the highest priority is 
      placed at front of list */
  list_insert_ordered(&t->locks_held,&(lock->elem),&lock_priority_cmp,NULL);

  intr_set_level (old_level);

}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success = false;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level;

  struct thread *t = thread_current();

  old_level = intr_disable();

  if (lock->holder == NULL) {
    success = true;
    lock->holder = t;
    thread_set_blocking_object(t,NULL,none);
    /* maintain invariant that lock with the highest priority is 
      placed at front of list */
    list_insert_ordered(&t->locks_held,&(lock->elem),&lock_priority_cmp,NULL);
  }

  intr_set_level (old_level);

  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  enum intr_level old_level;

  old_level = intr_disable ();

  lock->holder = NULL;
  if (!list_empty(&lock->waiters)) {
    /* Assume invariant that highest priority thread is always at the front of the 
       waiting list is correctly maintained. If this is true then all that needs to 
       be done here is to pop the first elements off the queue */
      thread_unblock(list_entry(list_pop_front(&lock->waiters),struct thread, elem));
  }

  /* Remove lock from list of locks held by the thread and degrade thread's priority 
     in case his current priority was donated by thread waiting on this lock */
  struct thread *t = thread_current();
  list_remove(&lock->elem);

  int highest_donated_priority = -1;

  if (!list_empty(&t->locks_held)) {
    highest_donated_priority = 
      list_entry(list_front(&t->locks_held),struct lock, elem)->priority;
  } 

  /* Maintain consistency between thread->priority and max of its original
     priority and the highest donated priority */
  if (t->original_priority > highest_donated_priority) {
    t->priority = t->original_priority;
    t->has_donation = false; 
  } else {
    t->priority = highest_donated_priority;
    ASSERT(t->has_donation == true);
  }

  /* Maintain consistency between lock->priority and the priority of the thread
     at the front of the waiting list */
  if (!list_empty(&lock->waiters)) {
    struct thread *top_waiter = (struct thread *)list_front(&lock->waiters);
    lock->priority = top_waiter->priority;
  } else {
    lock->priority = -1;
  }



  intr_set_level (old_level);

  /* Transfer control to scheduler in case a thread with higher priority has become ready*/
  thread_yield();

}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
    struct thread *waiting_thread;      /* Thread waiting for condition */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

bool
cond_priority_cmp(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED) 
{
  struct semaphore_elem *sa = list_entry(a,struct semaphore_elem, elem);
  struct semaphore_elem *sb = list_entry(b,struct semaphore_elem, elem);

  return sa->waiting_thread->priority > 
                  sb->waiting_thread->priority;
}


/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);

  waiter.waiting_thread = thread_current();

  list_insert_ordered (&cond->waiters, &waiter.elem,&cond_priority_cmp,NULL);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
