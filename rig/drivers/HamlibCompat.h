#ifndef RIG_DRIVERS_HAMLIBCOMPAT_H
#define RIG_DRIVERS_HAMLIBCOMPAT_H

#define HAMLIB_VERSION_CHECK(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define HAMLIB_VERSION \
    HAMLIB_VERSION_CHECK(HAMLIBVERSION_MAJOR, \
                         HAMLIBVERSION_MINOR, \
                         HAMLIBVERSION_PATCH)

// The public accessor macros were introduced in Hamlib 4.6.
#if HAMLIB_VERSION >= HAMLIB_VERSION_CHECK(4, 6, 0)

#define QLOG_HAMLIB_STATE(r)    HAMLIB_STATE(r)
#define QLOG_HAMLIB_RIGPORT(r)  HAMLIB_RIGPORT(r)
#define QLOG_HAMLIB_PTTPORT(r)  HAMLIB_PTTPORT(r)
#define QLOG_HAMLIB_ROTPORT(r)  HAMLIB_ROTPORT(r)

#else

#define QLOG_HAMLIB_STATE(r)    (&(r)->state)
#define QLOG_HAMLIB_RIGPORT(r)  (&(r)->state.rigport)
#define QLOG_HAMLIB_PTTPORT(r)  (&(r)->state.pttport)
#define QLOG_HAMLIB_ROTPORT(r)  (&(r)->state.rotport)

#endif

#endif // RIG_DRIVERS_HAMLIBCOMPAT_H
