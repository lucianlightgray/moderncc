#ifndef MCCNAME_H
#define MCCNAME_H

#include <stdint.h>

typedef uint64_t MccName;

typedef enum MccNameTag {
	MCC_NS_NONE = 0,
	MCC_NS_AST_SLOT = 1,
	MCC_NS_CST_BRANCH = 2,
} MccNameTag;

static inline MccName mcc_name(uint32_t tag, uint32_t id) {
	return ((MccName)tag << 32) | (MccName)id;
}

static inline uint32_t mcc_name_tag(MccName n) {
	return (uint32_t)(n >> 32);
}

static inline uint32_t mcc_name_id(MccName n) {
	return (uint32_t)n;
}

#endif
