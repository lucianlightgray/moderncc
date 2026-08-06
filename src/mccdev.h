#ifndef MCC_DEV_H
#define MCC_DEV_H

#ifndef MCC_DEV
#define MCC_DEV 0
#endif

#if MCC_DEV
#define MCC_DEV_ENV_ON(name) mcc_env_on(name)
#define MCC_DEV_ENV(name) getenv(name)
#else
#define MCC_DEV_ENV_ON(name) 0
#define MCC_DEV_ENV(name) ((const char *)0)
#endif

#endif
