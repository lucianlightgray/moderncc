#ifndef MCC_DEV_H
#define MCC_DEV_H

#ifndef MCC_DEV
#define MCC_DEV 0
#endif

#include <stdlib.h>

static inline int mcc_dev_enabled(void) {
	static int mcc_dev_cached = -1;
	if (mcc_dev_cached < 0) {
		const char *e = getenv("MCC_DEV");
		mcc_dev_cached = e && e[0] && e[0] != '0';
	}
	return mcc_dev_cached;
}

#if MCC_DEV
#define MCC_DEV_ENV_ON(name) mcc_env_on(name)
#define MCC_DEV_ENV(name) getenv(name)
#else
#define MCC_DEV_ENV_ON(name) 0
#define MCC_DEV_ENV(name) ((const char *)0)
#endif

#endif
