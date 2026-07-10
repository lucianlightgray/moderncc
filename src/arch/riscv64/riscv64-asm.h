#ifndef MCC_RISCV64_ASM_H
#define MCC_RISCV64_ASM_H

#ifndef MCC_DISABLE_ASM
#define CONFIG_MCC_ASM
#endif
#define NB_ASM_REGS 64

ST_FUNC void g(int c);
ST_FUNC void gen_le16(int c);
ST_FUNC void gen_le32(int c);

#endif
