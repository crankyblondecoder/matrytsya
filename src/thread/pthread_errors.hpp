#ifndef PTHREAD_ERRORS_H
#define PTHREAD_ERRORS_H

// Error strings corresponding to Pthread error codes.
// The meaning of these codes _is_ function specific.
// Centralised to avoid duplication of these strings where they need to be share across classes.

// Mutex init related.
#define PTHREAD_MUTEX_INIT_EAGAIN "EAGAIN The system lacked the necessary resources (other than memory)."
#define PTHREAD_MUTEX_INIT_ENOMEM "ENOMEM Insufficient memory exists to initialize the mutex."
#define PTHREAD_MUTEX_INIT_EPERM "EPERM The caller does not have the privilege to perform the operation."
#define PTHREAD_MUTEX_INIT_EBUSY "EBUSY attempt to reinitialise a not yet destroyed mutex."
#define PTHREAD_MUTEX_INIT_EINVAL "EINVAL The value specified by attr is invalid."

// Mutex lock related.
#define PTHREAD_MUTEX_LOCK_EINVAL "EINVAL The value specified by mutex does not refer to an initialized mutex object."
#define PTHREAD_MUTEX_LOCK_EAGAIN "EAGAIN The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded."
#define PTHREAD_MUTEX_LOCK_EDEADLK "EDEADLK The current thread already owns the mutex."

// Mutex unlock related.
#define PTHREAD_MUTEX_UNLOCK_EINVAL "EINVAL The value specified by mutex does not refer  to  an  initialized mutex object."
#define PTHREAD_MUTEX_UNLOCK_EAGAIN "EAGAIN The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded."
#define PTHREAD_MUTEX_UNLOCK_EPERM "EPERM The current thread does not own the mutex."

#endif