#ifndef MCC_ARM_ASM_H
#define MCC_ARM_ASM_H

#ifndef MCC_CONFIG_ASM
#define MCC_CONFIG_ASM 1
#endif
#define MCC_NB_ASM_REGS 16

ST_FUNC void g(int c);
ST_FUNC void gen_le16(int c);
ST_FUNC void gen_le32(int c);

#endif
