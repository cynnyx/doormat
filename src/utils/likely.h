#ifndef DOOR_MAT_LIKELY_H
#define DOOR_MAT_LIKELY_H

#undef LIKELY
#undef UNLIKELY

#if defined(__GNUC__) && __GNUC__ >= 4
#define LIKELY(x)   (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#endif //DOOR_MAT_LIKELY_H
