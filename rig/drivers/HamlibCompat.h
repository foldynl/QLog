#ifndef RIG_DRIVERS_HAMLIBCOMPAT_H
#define RIG_DRIVERS_HAMLIBCOMPAT_H

#define HAMLIB_VERSION_CHECK(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define HAMLIB_VERSION \
    HAMLIB_VERSION_CHECK(HAMLIBVERSION_MAJOR, \
                         HAMLIBVERSION_MINOR, \
                         HAMLIBVERSION_PATCH)

#endif // RIG_DRIVERS_HAMLIBCOMPAT_H
