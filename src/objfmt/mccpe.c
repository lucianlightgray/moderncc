#include "mcc.h"

#define PE_MERGE_DATA 1
#define PE_PRINT_SECTIONS 0

#ifndef _WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#include <sys/stat.h>
#else
#include <process.h>
#endif

#ifdef MCC_TARGET_X86_64
# define ADDR3264 ULONGLONG
# define PE_IMAGE_REL IMAGE_REL_BASED_DIR64
# define REL_TYPE_DIRECT R_X86_64_64
# define R_XXX_THUNKFIX R_X86_64_PC32
# define R_XXX_RELATIVE R_X86_64_RELATIVE
# define R_XXX_FUNCCALL R_X86_64_PLT32
# define IMAGE_FILE_MACHINE 0x8664
# define RSRC_RELTYPE 3

#elif defined MCC_TARGET_ARM
# define ADDR3264 DWORD
# define PE_IMAGE_REL IMAGE_REL_BASED_HIGHLOW
# define REL_TYPE_DIRECT R_ARM_ABS32
# define R_XXX_THUNKFIX R_ARM_ABS32
# define R_XXX_RELATIVE R_ARM_RELATIVE
# define R_XXX_FUNCCALL R_ARM_PC24
# define R_XXX_FUNCCALL2 R_ARM_ABS32
# define IMAGE_FILE_MACHINE 0x01C0
# define RSRC_RELTYPE 7

#elif defined MCC_TARGET_ARM64
# define ADDR3264 ULONGLONG
# define PE_IMAGE_REL IMAGE_REL_BASED_DIR64
# define REL_TYPE_DIRECT R_AARCH64_ABS64
# define R_XXX_THUNKFIX R_AARCH64_ABS64
# define R_XXX_RELATIVE R_AARCH64_RELATIVE
# define R_XXX_FUNCCALL R_AARCH64_CALL26
# define IMAGE_FILE_MACHINE 0xAA64
# define RSRC_RELTYPE 3

#elif defined MCC_TARGET_I386
# define ADDR3264 DWORD
# define PE_IMAGE_REL IMAGE_REL_BASED_HIGHLOW
# define REL_TYPE_DIRECT R_386_32
# define R_XXX_THUNKFIX R_386_32
# define R_XXX_RELATIVE R_386_RELATIVE
# define R_XXX_FUNCCALL R_386_PC32
# define IMAGE_FILE_MACHINE 0x014C
# define RSRC_RELTYPE 7

#endif

#ifndef IMAGE_NT_SIGNATURE

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long long ULONGLONG;
#pragma pack(push, 1)

typedef struct _IMAGE_DOS_HEADER {
    WORD e_magic;
    WORD e_cblp;
    WORD e_cp;
    WORD e_crlc;
    WORD e_cparhdr;
    WORD e_minalloc;
    WORD e_maxalloc;
    WORD e_ss;
    WORD e_sp;
    WORD e_csum;
    WORD e_ip;
    WORD e_cs;
    WORD e_lfarlc;
    WORD e_ovno;
    WORD e_res[4];
    WORD e_oemid;
    WORD e_oeminfo;
    WORD e_res2[10];
    DWORD e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

#define IMAGE_NT_SIGNATURE  0x00004550
#define SIZE_OF_NT_SIGNATURE 4

typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;
    WORD    NumberOfSections;
    DWORD   TimeDateStamp;
    DWORD   PointerToSymbolTable;
    DWORD   NumberOfSymbols;
    WORD    SizeOfOptionalHeader;
    WORD    Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;


#define IMAGE_SIZEOF_FILE_HEADER 20

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD   VirtualAddress;
    DWORD   Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;


typedef struct _IMAGE_OPTIONAL_HEADER {
    WORD    Magic;
    BYTE    MajorLinkerVersion;
    BYTE    MinorLinkerVersion;
    DWORD   SizeOfCode;
    DWORD   SizeOfInitializedData;
    DWORD   SizeOfUninitializedData;
    DWORD   AddressOfEntryPoint;
    DWORD   BaseOfCode;
#if !defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_ARM64)
    DWORD   BaseOfData;
#endif
    ADDR3264 ImageBase;
    DWORD   SectionAlignment;
    DWORD   FileAlignment;
    WORD    MajorOperatingSystemVersion;
    WORD    MinorOperatingSystemVersion;
    WORD    MajorImageVersion;
    WORD    MinorImageVersion;
    WORD    MajorSubsystemVersion;
    WORD    MinorSubsystemVersion;
    DWORD   Win32VersionValue;
    DWORD   SizeOfImage;
    DWORD   SizeOfHeaders;
    DWORD   CheckSum;
    WORD    Subsystem;
    WORD    DllCharacteristics;
    ADDR3264 SizeOfStackReserve;
    ADDR3264 SizeOfStackCommit;
    ADDR3264 SizeOfHeapReserve;
    ADDR3264 SizeOfHeapCommit;
    DWORD   LoaderFlags;
    DWORD   NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER32, IMAGE_OPTIONAL_HEADER64, IMAGE_OPTIONAL_HEADER;

#define IMAGE_DIRECTORY_ENTRY_EXPORT          0
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8
#define IMAGE_DIRECTORY_ENTRY_TLS             9
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11
#define IMAGE_DIRECTORY_ENTRY_IAT            12
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14

#define IMAGE_SIZEOF_SHORT_NAME         8

typedef struct _IMAGE_SECTION_HEADER {
    BYTE    Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
            DWORD   PhysicalAddress;
            DWORD   VirtualSize;
    } Misc;
    DWORD   VirtualAddress;
    DWORD   SizeOfRawData;
    DWORD   PointerToRawData;
    DWORD   PointerToRelocations;
    DWORD   PointerToLinenumbers;
    WORD    NumberOfRelocations;
    WORD    NumberOfLinenumbers;
    DWORD   Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

#define IMAGE_SIZEOF_SECTION_HEADER     40

typedef struct _IMAGE_EXPORT_DIRECTORY {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD MajorVersion;
    WORD MinorVersion;
    DWORD Name;
    DWORD Base;
    DWORD NumberOfFunctions;
    DWORD NumberOfNames;
    DWORD AddressOfFunctions;
    DWORD AddressOfNames;
    DWORD AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY,*PIMAGE_EXPORT_DIRECTORY;

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
    union {
        DWORD Characteristics;
        DWORD OriginalFirstThunk;
    };
    DWORD TimeDateStamp;
    DWORD ForwarderChain;
    DWORD Name;
    DWORD FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR;

typedef struct _IMAGE_BASE_RELOCATION {
    DWORD   VirtualAddress;
    DWORD   SizeOfBlock;
} IMAGE_BASE_RELOCATION;

#define IMAGE_SIZEOF_BASE_RELOCATION     8

#ifndef IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA
#define IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA
#endif
#ifndef IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
#define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE PE_DLLCHARACTERISTICS_DYNAMIC_BASE
#endif
#ifndef IMAGE_DLLCHARACTERISTICS_NX_COMPAT
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT PE_DLLCHARACTERISTICS_NX_COMPAT
#endif
#ifndef IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE
#define IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE
#endif

#define IMAGE_REL_BASED_ABSOLUTE         0
#define IMAGE_REL_BASED_HIGH             1
#define IMAGE_REL_BASED_LOW              2
#define IMAGE_REL_BASED_HIGHLOW          3
#define IMAGE_REL_BASED_HIGHADJ          4
#define IMAGE_REL_BASED_MIPS_JMPADDR     5
#define IMAGE_REL_BASED_SECTION          6
#define IMAGE_REL_BASED_REL32            7
#define IMAGE_REL_BASED_DIR64           10

#define IMAGE_SCN_CNT_CODE                  0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA      0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA    0x00000080
#define IMAGE_SCN_MEM_DISCARDABLE           0x02000000
#define IMAGE_SCN_MEM_SHARED                0x10000000
#define IMAGE_SCN_MEM_EXECUTE               0x20000000
#define IMAGE_SCN_MEM_READ                  0x40000000
#define IMAGE_SCN_MEM_WRITE                 0x80000000

#pragma pack(pop)

#endif

static WORD pe_get_dll_characteristics(MCCState *s1)
{
    unsigned v = 0;

#ifdef MCC_TARGET_ARM64
    v = PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA |
        PE_DLLCHARACTERISTICS_DYNAMIC_BASE |
        PE_DLLCHARACTERISTICS_NX_COMPAT |
        PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE;
#endif
    v |= s1->pe_dll_characteristics;
    v &= ~s1->pe_dll_characteristics_clear;
    return v;
}

#ifndef IMAGE_FILE_MACHINE_ARM64
#define IMAGE_FILE_MACHINE_ARM64 0xAA64
#endif
#ifndef IMAGE_REL_BASED_DIR64
# define IMAGE_REL_BASED_DIR64 10
#endif

#pragma pack(push, 1)
struct pe_header
{
    IMAGE_DOS_HEADER doshdr;
    BYTE dosstub[0x40];
    DWORD nt_sig;
    IMAGE_FILE_HEADER filehdr;
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
    IMAGE_OPTIONAL_HEADER64 opthdr;
#else
#ifdef _WIN64
    IMAGE_OPTIONAL_HEADER32 opthdr;
#else
    IMAGE_OPTIONAL_HEADER opthdr;
#endif
#endif
};

struct pe_reloc_header {
    DWORD offset;
    DWORD size;
};

struct pe_rsrc_header {
    struct _IMAGE_FILE_HEADER filehdr;
    struct _IMAGE_SECTION_HEADER sectionhdr;
};

struct pe_rsrc_reloc {
    DWORD offset;
    DWORD size;
    WORD type;
};
#pragma pack(pop)


enum {
    sec_text = 0,
    sec_rdata ,
    sec_data ,
    sec_bss ,
    sec_idata ,
    sec_pdata ,
    sec_other ,
    sec_rsrc ,
    sec_debug ,
    sec_reloc ,
    sec_last
};

#if 0
static const DWORD pe_sec_flags[] = {
    0x60000020,
    0xC0000040,
    0xC0000080,
    0x40000040,
    0x40000040,
    0xE0000060,
    0x40000040,
    0x42000802,
    0x42000040,
};
#endif

struct section_info {
    int cls;
    char name[32];
    ADDR3264 sh_addr;
    DWORD sh_size;
    DWORD pe_flags;
    Section *sec;
    DWORD data_size;
    IMAGE_SECTION_HEADER ish;
};

struct import_symbol {
    int sym_index;
    int iat_index;
    int thk_offset;
};

struct pe_import_info {
    int dll_index;
    int sym_count;
    struct import_symbol **symbols;
};

struct pe_info {
    MCCState *s1;
    Section *reloc;
    Section *thunk;
    Section *coffsym;
    Section *coffstr;
    const char *filename;
    int type;
    DWORD sizeofheaders;
    ADDR3264 imagebase;
    const char *start_symbol;
    DWORD start_addr;
    DWORD imp_offs;
    DWORD imp_size;
    DWORD iat_offs;
    DWORD iat_size;
    DWORD exp_offs;
    DWORD exp_size;
    int subsystem;
    DWORD section_align;
    DWORD file_align;
    struct section_info **sec_info;
    int sec_count;
    struct pe_import_info **imp_info;
    int imp_count;
    FILE *op;
    DWORD sum;
    unsigned pos;
};

#define PE_NUL 0
#define PE_DLL 1
#define PE_GUI 2
#define PE_EXE 3
#define PE_RUN 4


static const char *pe_export_name(MCCState *s1, ElfW(Sym) *sym)
{
    const char *name = (char*)symtab_section->link->data + sym->st_name;
    if (s1->leading_underscore && name[0] == '_' && !(sym->st_other & ST_PE_STDCALL))
        return name + 1;
    return name;
}


static int dynarray_assoc(void **pp, int n, int key)
{
    for (int i = 0; i < n; ++i, ++pp)
    if (key == **(int **) pp)
        return i;
    return -1;
}

static DWORD umin(DWORD a, DWORD b)
{
    return a < b ? a : b;
}

static DWORD umax(DWORD a, DWORD b)
{
    return a < b ? b : a;
}

static DWORD pe_file_align(struct pe_info *pe, DWORD n)
{
    return (n + (pe->file_align - 1)) & ~(pe->file_align - 1);
}

static ADDR3264 pe_virtual_align(struct pe_info *pe, ADDR3264 n)
{
    return (n + (pe->section_align - 1)) & ~(ADDR3264)(pe->section_align - 1);
}

static void pe_align_section(Section *s, int a)
{
    int i = s->data_offset & (a-1);
    if (i)
        section_ptr_add(s, a - i);
}

static void pe_set_datadir(struct pe_header *hdr, int dir, DWORD addr, DWORD size)
{
    hdr->opthdr.DataDirectory[dir].VirtualAddress = addr;
    hdr->opthdr.DataDirectory[dir].Size = size;
}

static int pe_fwrite(struct pe_info *pe, const void *data, int len)
{
    const WORD *p = data;
    DWORD sum;
    int ret;
    pe->pos += (ret = fwrite(data, 1, len, pe->op));
    sum = pe->sum;
    for (int i = len; i > 0; i -= 2) {
        sum += (i >= 2) ? *p++ : *(BYTE*)p;
        sum = (sum + (sum >> 16)) & 0xFFFF;
    }
    pe->sum = sum;
    return len == ret ? 0 : -1;
}

static void pe_fpad(struct pe_info *pe, DWORD new_pos)
{
    char buf[256];
    int n, diff = new_pos - pe->pos;
    memset(buf, 0, sizeof buf);
    while (diff > 0) {
        diff -= n = umin(diff, sizeof buf);
        fwrite(buf, n, 1, pe->op);
    }
    pe->pos = new_pos;
}


#pragma pack(push, 1)
struct syment
{
    union {
        char        n_name[8];
        struct {
            int32_t n_zeroes;
            int32_t n_offset;
        };
    };
    int32_t         n_value;
    short           n_scnum;
    unsigned short  n_type;
    char            n_sclass;
    char            n_numaux;
};
#pragma pack(pop)

#define SHF_PRIVATE 0x80000000

static void pe_add_coffsym(struct pe_info *pe)
{
    MCCState *s1 = pe->s1;
    ElfSym *esym;
    struct syment *se;

    if (NULL == pe->coffsym) {
        pe->coffsym = new_section(s1, ".coffsym", SHT_PROGBITS, SHF_PRIVATE);
        pe->coffstr = new_section(s1, ".coffstr", SHT_PROGBITS, SHF_PRIVATE);
        section_ptr_add(pe->coffstr, 4);
        return;
    }

#if 0
    se = section_ptr_add(pe->coffsym, sizeof *se);
    strcpy(se->n_name, ".file");
    se->n_scnum = -2;
    se->n_sclass = 0x67;
    se->n_numaux = 1;
    se = section_ptr_add(pe->coffsym, sizeof *se);
    strcpy((char*)se, "no-file");
#endif

#if 1
    esym = (ElfSym*)s1->symtab->data;
    for (int n = s1->symtab->data_offset / sizeof *esym; ++esym, --n;) {
        int sym_bind = ELFW(ST_BIND)(esym->st_info);
        if (sym_bind == STB_GLOBAL) {
            char *name = esym->st_name + (char*)s1->symtab->link->data;
            int nl = strlen(name);
            addr_t value = esym->st_value;
            int shnum = esym->st_shndx;
            if (shnum != SHN_UNDEF && shnum < s1->nb_sections) {
                Section *s = s1->sections[shnum];
                shnum = s->sh_info;
                value = value - s->sh_addr;
            }
            se = section_ptr_add(pe->coffsym, sizeof *se);
            se->n_value = value;
            se->n_scnum = shnum;
            se->n_sclass = 2;
            if (nl <= 8)
                memcpy(se->n_name, name, nl);
            else
                se->n_offset = put_elf_str(pe->coffstr, name);
        }
    }
#endif
    write32le(pe->coffstr->data, pe->coffstr->data_offset);
}

#ifndef _WIN32
static void pe_shell_quote(CString *cmd, const char *arg)
{
    cstr_cat(cmd, "'", 1);
    while (*arg) {
        if (*arg == '\'')
            cstr_cat(cmd, "'\\''", 4);
        else
            cstr_cat(cmd, arg, 1);
        ++arg;
    }
    cstr_cat(cmd, "'", 1);
}
#endif

static intptr_t pe_run_cv2pdb(const char *exename)
{
#ifdef _WIN32
    const char *argv[] = { "cv2pdb.exe", exename, NULL };
    return _spawnvp(_P_WAIT, "cv2pdb.exe", argv);
#else
    CString cmd;
    intptr_t ret;

    cstr_new(&cmd);
    cstr_cat(&cmd, "cv2pdb.exe ", -1);
    pe_shell_quote(&cmd, exename);
    cstr_ccat(&cmd, 0);
    ret = system(cmd.data);
    cstr_free(&cmd);
    return ret;
#endif
}

static void pe_create_pdb(MCCState *s1, const char *exename)
{
    size_t len = strlen(exename);
    char *pdbfile = mcc_malloc(len + sizeof(".pdb"));
    intptr_t r;

    strcpy(pdbfile, exename);
    strcpy(mcc_fileextension(pdbfile), ".pdb");
    r = pe_run_cv2pdb(exename);
    if (r) {
        mcc_error_noabort("could not create '%s'\n(need working cv2pdb from https://github.com/rainers/cv2pdb)", pdbfile);
    } else if (s1->verbose) {
        printf("<- %s\n", pdbfile);
    }
    mcc_free(pdbfile);
}

static int pe_write(struct pe_info *pe)
{
    static const struct pe_header pe_template = {
    {
    0x5A4D,
    0x0090,
    0x0003,
    0x0000,

    0x0004,
    0x0000,
    0xFFFF,
    0x0000,

    0x00B8,
    0x0000,
    0x0000,
    0x0000,
    0x0040,
    0x0000,
    {0,0,0,0},
    0x0000,
    0x0000,
    {0,0,0,0,0,0,0,0,0,0},
    0x00000080
    },{
    0x0e,0x1f,0xba,0x0e,0x00,0xb4,0x09,0xcd,0x21,0xb8,0x01,0x4c,0xcd,0x21,0x54,0x68,
    0x69,0x73,0x20,0x70,0x72,0x6f,0x67,0x72,0x61,0x6d,0x20,0x63,0x61,0x6e,0x6e,0x6f,
    0x74,0x20,0x62,0x65,0x20,0x72,0x75,0x6e,0x20,0x69,0x6e,0x20,0x44,0x4f,0x53,0x20,
    0x6d,0x6f,0x64,0x65,0x2e,0x0d,0x0d,0x0a,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    },
    0x00004550,
    {
    IMAGE_FILE_MACHINE,
    0x0003,
    0x00000000,
    0x00000000,
    0x00000000,
#if defined(MCC_TARGET_X86_64)
    0x00F0,
    0x022F
#define CHARACTERISTICS_DLL 0x222E
#elif defined(MCC_TARGET_I386)
    0x00E0,
    0x030F
#define CHARACTERISTICS_DLL 0x230E
#elif defined(MCC_TARGET_ARM)
    0x00E0,
    0x010F,
#define CHARACTERISTICS_DLL 0x230F
#elif defined(MCC_TARGET_ARM64)
    0x00F0,
    0x0022
#define CHARACTERISTICS_DLL 0x2022
#endif
},{
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
    0x020B,
#else
    0x010B,
#endif
    0x06,
    0x00,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
#if !defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_ARM64)
    0x00000000,
#endif
#if defined(MCC_TARGET_ARM)
    0x00100000,
#elif defined(MCC_TARGET_ARM64)
    0x140000000ULL,
#else
    0x00400000,
#endif
    0x00001000,
    0x00000200,
#if defined(MCC_TARGET_ARM64)
    0x0006,
    0x0002,
#else
    0x0004,
    0x0000,
#endif
    0x0000,
    0x0000,
#if defined(MCC_TARGET_ARM64)
    0x0006,
    0x0002,
#else
    0x0004,
    0x0000,
#endif
    0x00000000,
    0x00000000,
    0x00000200,
    0x00000000,
    0x0002,
    0x0000,
    0x00100000,
    0x00001000,
    0x00100000,
    0x00001000,
    0x00000000,
    0x00000010,

    {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
     {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}}
    }};

    struct pe_header pe_header = pe_template;

    DWORD file_offset;
    struct section_info *si;
    IMAGE_SECTION_HEADER *psh;
    MCCState *s1 = pe->s1;

    if (s1->do_debug)
        pe_add_coffsym(pe);

    pe->op = fopen(pe->filename, "wb");
    if (NULL == pe->op)
        return mcc_error_noabort("could not write '%s': %s", pe->filename, strerror(errno));

    pe->sizeofheaders = pe_file_align(pe,
        sizeof (struct pe_header)
        + pe->sec_count * sizeof (IMAGE_SECTION_HEADER)
        );

    file_offset = pe->sizeofheaders;

    if (2 == s1->verbose)
        printf("-------------------------------"
               "\n  virt   file   size  section" "\n");
    for (int i = 0; i < pe->sec_count; ++i) {
        DWORD addr, size;
        const char *sh_name;

        si = pe->sec_info[i];
        sh_name = si->name;
        addr = si->sh_addr - pe->imagebase;
        size = si->sh_size;
        psh = &si->ish;

        if (2 == s1->verbose)
            printf("%6x %6x %6x  %s\n",
                (unsigned)addr, (unsigned)file_offset, (unsigned)size, sh_name);

        switch (si->cls) {
            case sec_text:
                if (!pe_header.opthdr.BaseOfCode)
                    pe_header.opthdr.BaseOfCode = addr;
                break;

            case sec_data:
#if !defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_ARM64)
                if (!pe_header.opthdr.BaseOfData)
                    pe_header.opthdr.BaseOfData = addr;
#endif
                break;

            case sec_bss:
                break;

            case sec_reloc:
                pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_BASERELOC, addr, size);
                break;

            case sec_rsrc:
                pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_RESOURCE, addr, size);
                break;

            case sec_pdata:
                pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_EXCEPTION, addr, size);
                break;
        }

        if (pe->imp_size) {
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_IMPORT,
                pe->imp_offs, pe->imp_size);
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_IAT,
                pe->iat_offs, pe->iat_size);
        }
        if (pe->exp_size) {
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_EXPORT,
                pe->exp_offs, pe->exp_size);
        }

        memcpy(psh->Name, sh_name, umin(strlen(sh_name), sizeof psh->Name));
        if (pe->coffstr && strlen(sh_name) > 8) {
            snprintf((char*)psh->Name, 8, "/%d", put_elf_str(pe->coffstr, sh_name));
        }

        psh->Characteristics = si->pe_flags;
        psh->VirtualAddress = addr;
        psh->Misc.VirtualSize = size;
        pe_header.opthdr.SizeOfImage =
            umax(pe_virtual_align(pe, size + addr), pe_header.opthdr.SizeOfImage);

        if (si->data_size) {
            psh->PointerToRawData = file_offset;
            file_offset = pe_file_align(pe, file_offset + si->data_size);
            psh->SizeOfRawData = file_offset - psh->PointerToRawData;
            if (si->cls == sec_text)
                pe_header.opthdr.SizeOfCode += psh->SizeOfRawData;
            else
                pe_header.opthdr.SizeOfInitializedData += psh->SizeOfRawData;
        }
    }

    pe_header.filehdr.NumberOfSections = pe->sec_count;
    pe_header.opthdr.AddressOfEntryPoint = pe->start_addr;
    pe_header.opthdr.SizeOfHeaders = pe->sizeofheaders;
    pe_header.opthdr.ImageBase = pe->imagebase;
    pe_header.opthdr.Subsystem = pe->subsystem;
    pe_header.opthdr.DllCharacteristics = pe_get_dll_characteristics(s1);
    if (s1->pe_stack_size)
        pe_header.opthdr.SizeOfStackReserve = s1->pe_stack_size;
    if (PE_DLL == pe->type)
        pe_header.filehdr.Characteristics = CHARACTERISTICS_DLL;
    pe_header.filehdr.Characteristics |= s1->pe_characteristics;
    if (pe->reloc)
        pe_header.filehdr.Characteristics &= ~PE_IMAGE_FILE_RELOCS_STRIPPED;

    if (pe->coffsym) {
        pe_add_coffsym(pe);
        pe_header.filehdr.PointerToSymbolTable = file_offset;
        pe_header.filehdr.NumberOfSymbols
            = pe->coffsym->data_offset / sizeof (struct syment);
    }

    pe_fwrite(pe, &pe_header, sizeof pe_header);
    for (int i = 0; i < pe->sec_count; ++i)
        pe_fwrite(pe, &pe->sec_info[i]->ish, sizeof(IMAGE_SECTION_HEADER));

    file_offset = pe->sizeofheaders;
    for (int i = 0; i < pe->sec_count; ++i) {
        Section *s;
        si = pe->sec_info[i];
        if (!si->data_size)
            continue;
        for (s = si->sec; s; s = s->prev) {
            pe_fpad(pe, file_offset);
            pe_fwrite(pe, s->data, s->data_offset);
            if (s->prev)
                file_offset += s->prev->sh_addr - s->sh_addr;
        }
        file_offset = si->ish.PointerToRawData + si->ish.SizeOfRawData;
        pe_fpad(pe, file_offset);
    }

    if (pe->coffsym) {
        pe_fwrite(pe, pe->coffsym->data, pe->coffsym->data_offset);
        pe_fwrite(pe, pe->coffstr->data, pe->coffstr->data_offset);
        file_offset = pe->pos;
    }

    pe->sum += file_offset;
    fseek(pe->op, offsetof(struct pe_header, opthdr.CheckSum), SEEK_SET);
    pe_fwrite(pe, &pe->sum, sizeof (DWORD));

    fclose (pe->op);
#ifndef _WIN32
    chmod(pe->filename, 0777);
#endif

    if (2 == s1->verbose)
        printf("-------------------------------\n");
    if (s1->verbose)
        printf("<- %s (%u bytes)\n", pe->filename, (unsigned)file_offset);

    if (s1->do_debug & 16)
        pe_create_pdb(s1, pe->filename);
    return 0;
}


static struct import_symbol *pe_add_import(struct pe_info *pe, int sym_index)
{
    int i;
    int dll_index;
    struct pe_import_info *p;
    struct import_symbol *s;
    ElfW(Sym) *isym;

    isym = (ElfW(Sym) *)pe->s1->dynsymtab_section->data + sym_index;
    dll_index = isym->st_size;

    i = dynarray_assoc ((void**)pe->imp_info, pe->imp_count, dll_index);
    if (-1 != i) {
        p = pe->imp_info[i];
        goto found_dll;
    }
    p = mcc_mallocz(sizeof *p);
    p->dll_index = dll_index;
    dynarray_add(&pe->imp_info, &pe->imp_count, p);

found_dll:
    i = dynarray_assoc ((void**)p->symbols, p->sym_count, sym_index);
    if (-1 != i)
        return p->symbols[i];

    s = mcc_mallocz(sizeof *s);
    dynarray_add(&p->symbols, &p->sym_count, s);
    s->sym_index = sym_index;
    return s;
}

static void pe_free_imports(struct pe_info *pe)
{
    for (int i = 0; i < pe->imp_count; ++i) {
        struct pe_import_info *p = pe->imp_info[i];
        dynarray_reset(&p->symbols, &p->sym_count);
    }
    dynarray_reset(&pe->imp_info, &pe->imp_count);
}

static void pe_build_imports(struct pe_info *pe)
{
    int thk_ptr, ent_ptr, dll_ptr, sym_cnt, i;
    DWORD rva_base = pe->thunk->sh_addr - pe->imagebase;
    int ndlls = pe->imp_count;
    MCCState *s1 = pe->s1;

    for (sym_cnt = i = 0; i < ndlls; ++i)
        sym_cnt += pe->imp_info[i]->sym_count;

    if (0 == sym_cnt)
        return;

    pe_align_section(pe->thunk, 16);
    pe->imp_size = (ndlls + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    pe->iat_size = (sym_cnt + ndlls) * sizeof(ADDR3264);
    dll_ptr = pe->thunk->data_offset;
    thk_ptr = dll_ptr + pe->imp_size;
    ent_ptr = thk_ptr + pe->iat_size;
    pe->imp_offs = dll_ptr + rva_base;
    pe->iat_offs = thk_ptr + rva_base;
    section_ptr_add(pe->thunk, pe->imp_size + 2*pe->iat_size);

    for (i = 0; i < pe->imp_count; ++i) {
        IMAGE_IMPORT_DESCRIPTOR *hdr;
        int dllindex;
        ADDR3264 v;
        struct pe_import_info *p = pe->imp_info[i];
        const char *name;
        DLLReference *dllref;

        dllindex = p->dll_index;
        if (dllindex)
            name = mcc_basename((dllref = s1->loaded_dlls[dllindex-1])->name);
        else
            name = "", dllref = NULL;

        v = put_elf_str(pe->thunk, name);
        hdr = (IMAGE_IMPORT_DESCRIPTOR*)(pe->thunk->data + dll_ptr);
        hdr->FirstThunk = thk_ptr + rva_base;
        hdr->OriginalFirstThunk = ent_ptr + rva_base;
        hdr->Name = v + rva_base;

        for (int k = 0, n = p->sym_count; k <= n; ++k) {
            if (k < n) {
                int iat_index = p->symbols[k]->iat_index;
                int sym_index = p->symbols[k]->sym_index;
                ElfW(Sym) *imp_sym = (ElfW(Sym) *)s1->dynsymtab_section->data + sym_index;
                const char *name = (char*)s1->dynsymtab_section->link->data + imp_sym->st_name;
                int ordinal;

                do {
                    ElfW(Sym) *esym = (ElfW(Sym) *)symtab_section->data + iat_index;
                    iat_index = esym->st_value;
                    esym->st_value = thk_ptr;
                    esym->st_shndx = pe->thunk->sh_num;
                } while (iat_index);

                if (dllref)
                    v = 0, ordinal = imp_sym->st_value;
                else
                    ordinal = 0, v = imp_sym->st_value;

#ifdef MCC_IS_NATIVE
                if (pe->type == PE_RUN) {
                    if (dllref) {
                        if ( !dllref->handle )
                            dllref->handle = LoadLibraryA(dllref->name);
                        v = (ADDR3264)GetProcAddress(dllref->handle, ordinal?(char*)0+ordinal:name);
                    }
                    if (!v)
                        mcc_error_noabort("could not resolve symbol '%s'", name);
                } else
#endif
                if (ordinal) {
                    v = ordinal | (ADDR3264)1 << (sizeof(ADDR3264)*8 - 1);
                } else {
                    v = pe->thunk->data_offset + rva_base;
                    section_ptr_add(pe->thunk, sizeof(WORD));
                    put_elf_str(pe->thunk, name);
                }

            } else {
                v = 0;
            }

            *(ADDR3264*)(pe->thunk->data+thk_ptr) =
            *(ADDR3264*)(pe->thunk->data+ent_ptr) = v;
            thk_ptr += sizeof (ADDR3264);
            ent_ptr += sizeof (ADDR3264);
        }
        dll_ptr += sizeof(IMAGE_IMPORT_DESCRIPTOR);
    }
}


struct pe_sort_sym
{
    int index;
    const char *name;
};

static int sym_cmp(const void *va, const void *vb)
{
    const char *ca = (*(struct pe_sort_sym**)va)->name;
    const char *cb = (*(struct pe_sort_sym**)vb)->name;
    return strcmp(ca, cb);
}

static void pe_build_exports(struct pe_info *pe)
{
    ElfW(Sym) *sym;
    int sym_index, sym_end;
    DWORD rva_base, base_o, func_o, name_o, ord_o, str_o;
    IMAGE_EXPORT_DIRECTORY *hdr;
    int sym_count;
    struct pe_sort_sym **sorted, *p;
    MCCState *s1 = pe->s1;

    FILE *op;
    char buf[260];
    const char *dllname;
    const char *name;

    rva_base = pe->thunk->sh_addr - pe->imagebase;
    sym_count = 0, sorted = NULL, op = NULL;

    sym_end = symtab_section->data_offset / sizeof(ElfW(Sym));
    for (sym_index = 1; sym_index < sym_end; ++sym_index) {
        sym = (ElfW(Sym)*)symtab_section->data + sym_index;
        name = pe_export_name(s1, sym);
        if (sym->st_other & ST_PE_EXPORT) {
            p = mcc_malloc(sizeof *p);
            p->index = sym_index;
            p->name = name;
            dynarray_add(&sorted, &sym_count, p);
        }
#if 0
        if (sym->st_other & ST_PE_EXPORT)
            printf("export: %s\n", name);
        if (sym->st_other & ST_PE_STDCALL)
            printf("stdcall: %s\n", name);
#endif
    }

    if (0 == sym_count)
        return;

    qsort (sorted, sym_count, sizeof *sorted, sym_cmp);

    pe_align_section(pe->thunk, 16);
    dllname = mcc_basename(pe->filename);

    base_o = pe->thunk->data_offset;
    func_o = base_o + sizeof(IMAGE_EXPORT_DIRECTORY);
    name_o = func_o + sym_count * sizeof (DWORD);
    ord_o = name_o + sym_count * sizeof (DWORD);
    str_o = ord_o + sym_count * sizeof(WORD);

    hdr = section_ptr_add(pe->thunk, str_o - base_o);
    hdr->Characteristics        = 0;
    hdr->Base                   = 1;
    hdr->NumberOfFunctions      = sym_count;
    hdr->NumberOfNames          = sym_count;
    hdr->AddressOfFunctions     = func_o + rva_base;
    hdr->AddressOfNames         = name_o + rva_base;
    hdr->AddressOfNameOrdinals  = ord_o + rva_base;
    hdr->Name                   = str_o + rva_base;
    put_elf_str(pe->thunk, dllname);

#if 1
    pstrcpy(buf, sizeof buf, pe->filename);
    strcpy(mcc_fileextension(buf), ".def");
    op = fopen(buf, "wb");
    if (NULL == op) {
        mcc_error_noabort("could not create '%s': %s", buf, strerror(errno));
    } else {
        fprintf(op, "LIBRARY %s\n\nEXPORTS\n", dllname);
        if (s1->verbose)
            printf("<- %s (%d symbol%s)\n", buf, sym_count, &"s"[sym_count < 2]);
    }
#endif

    for (int ord = 0; ord < sym_count; ++ord)
    {
        p = sorted[ord], sym_index = p->index, name = p->name;
        put_elf_reloc(symtab_section, pe->thunk,
            func_o, R_XXX_RELATIVE, sym_index);
        *(DWORD*)(pe->thunk->data + name_o)
            = pe->thunk->data_offset + rva_base;
        *(WORD*)(pe->thunk->data + ord_o)
            = ord;
        put_elf_str(pe->thunk, name);
        func_o += sizeof (DWORD);
        name_o += sizeof (DWORD);
        ord_o += sizeof (WORD);
        if (op)
            fprintf(op, "%s\n", name);
    }

    pe->exp_offs = base_o + rva_base;
    pe->exp_size = pe->thunk->data_offset - base_o;
    dynarray_reset(&sorted, &sym_count);
    if (op)
        fclose(op);
}

static void pe_build_reloc (struct pe_info *pe)
{
    DWORD offset, block_ptr, sh_addr, addr;
    int count, i;
    ElfW_Rel *rel, *rel_end;
    Section *s = NULL, *sr;
    struct pe_reloc_header *hdr;
    MCCState *s1 = pe->s1;
    int dwarf = 0, n;

    sh_addr = offset = block_ptr = count = i = 0;
    rel = rel_end = NULL;

    for(;;) {
        if (rel < rel_end) {
            int type = ELFW(R_TYPE)(rel->r_info);
            addr = rel->r_offset + sh_addr;
            ++ rel;
            if (type != REL_TYPE_DIRECT)
                continue;
            if (dwarf) {
                n = ((ElfSym *)s1->symtab->data + ELFW(R_SYM)(rel[-1].r_info))->st_shndx;
                if (n >= s1->dwlo && n < s1->dwhi)
                    continue;
            }
            if (count == 0) {
                block_ptr = pe->reloc->data_offset;
                section_ptr_add(pe->reloc, sizeof(struct pe_reloc_header));
                offset = addr & 0xFFFFFFFF<<12;
            }
            if ((addr -= offset)  < (1<<12)) {
                WORD *wp = section_ptr_add(pe->reloc, sizeof (WORD));
                *wp = addr | PE_IMAGE_REL<<12;
                ++count;
                continue;
            }
            -- rel;

        } else if (s) {
            sr = s->reloc;
            if (sr) {
                rel = (ElfW_Rel *)sr->data;
                rel_end = (ElfW_Rel *)(sr->data + sr->data_offset);
                sh_addr = s->sh_addr;
                dwarf = s->sh_num >= s1->dwlo && s->sh_num < s1->dwhi;
            }
            s = s->prev;
            continue;

        } else if (i < pe->sec_count) {
            s = pe->sec_info[i]->sec, ++i;
            continue;

        } else if (!count)
            break;

        if (count & 1)
            section_ptr_add(pe->reloc, sizeof(WORD)), ++count;
        hdr = (struct pe_reloc_header *)(pe->reloc->data + block_ptr);
        hdr -> offset = offset - pe->imagebase;
        hdr -> size = count * sizeof(WORD) + sizeof(struct pe_reloc_header);
        count = 0;
    }
}

static int pe_section_class(Section *s)
{
    int type, flags;
    const char *name;
    type = s->sh_type;
    flags = s->sh_flags;
    name = s->name;

    if (0 == memcmp(name, ".stab", 5) || 0 == memcmp(name, ".debug_", 7)) {
        return sec_debug;
    } else if (flags & SHF_ALLOC) {
        if (type == SHT_PROGBITS
         || type == SHT_INIT_ARRAY
         || type == SHT_FINI_ARRAY) {
            if (flags & SHF_EXECINSTR)
                return sec_text;
            if (flags & SHF_WRITE)
                return sec_data;
            if (0 == strcmp(name, ".rsrc"))
                return sec_rsrc;
            if (0 == strcmp(name, ".iedat"))
                return sec_idata;
            if (0 == strcmp(name, ".pdata"))
                return sec_pdata;
            return sec_rdata;
        } else if (type == SHT_NOBITS) {
            return sec_bss;
        }
        return sec_other;
    } else {
        if (0 == strcmp(name, ".reloc"))
            return sec_reloc;
    }
    return sec_last;
}

static int pe_assign_addresses (struct pe_info *pe)
{
    int i, k, n, c, nbs;
    ADDR3264 addr;
    int *sec_order, *sec_cls;
    struct section_info *si;
    Section *s;
    MCCState *s1 = pe->s1;

    if (PE_DLL == pe->type ||
        (pe_get_dll_characteristics(s1) & PE_DLLCHARACTERISTICS_DYNAMIC_BASE))
        pe->reloc = new_section(s1, ".reloc", SHT_PROGBITS, 0);

    nbs = s1->nb_sections;
    sec_order = mcc_mallocz(2 * sizeof (int) * nbs);
    sec_cls = sec_order + nbs;
    for (i = 1; i < nbs; ++i) {
        s = s1->sections[i];
        k = pe_section_class(s);
        for (n = i; n > 1 && k < (c = sec_cls[n - 1]); --n)
            sec_cls[n] = c, sec_order[n] = sec_order[n - 1];
        sec_cls[n] = k, sec_order[n] = i;
    }
    si = NULL;
    addr = pe->imagebase + 1;

    for (i = 1; (c = sec_cls[i]) < sec_last; ++i) {
        s = s1->sections[sec_order[i]];

        if (PE_MERGE_DATA && c == sec_bss)
            c = sec_data;

        if (si && c == si->cls && c != sec_debug) {
            s->sh_addr = addr = ((addr - 1) | (16 - 1)) + 1;
        } else {
            si = NULL;
            s->sh_addr = addr = pe_virtual_align(pe, addr);
        }

        if (NULL == pe->thunk
            && c == (data_section == rodata_section ? sec_data : sec_rdata))
            pe->thunk = s;

        if (s == pe->thunk) {
            pe_build_imports(pe);
            pe_build_exports(pe);
        }
        if (s == pe->reloc)
            pe_build_reloc (pe);

        if (0 == s->data_offset)
            continue;

        if (si)
            goto add_section;

        si = mcc_mallocz(sizeof *si);
        dynarray_add(&pe->sec_info, &pe->sec_count, si);

        strcpy(si->name, s->name);
        si->cls = c;
        si->sh_addr = addr;

        si->pe_flags = IMAGE_SCN_MEM_READ;
        if (s->sh_flags & SHF_EXECINSTR)
            si->pe_flags |= IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE;
        else if (s->sh_type == SHT_NOBITS)
            si->pe_flags |= IMAGE_SCN_CNT_UNINITIALIZED_DATA;
        else
            si->pe_flags |= IMAGE_SCN_CNT_INITIALIZED_DATA;
        if (s->sh_flags & SHF_WRITE)
            si->pe_flags |= IMAGE_SCN_MEM_WRITE;
        if (0 == (s->sh_flags & SHF_ALLOC))
            si->pe_flags |= IMAGE_SCN_MEM_DISCARDABLE;

add_section:
        s->sh_info = pe->sec_count;
        addr += s->data_offset;
        si->sh_size = addr - si->sh_addr;
        if (s->sh_type != SHT_NOBITS) {
            Section **ps = &si->sec;
            while (*ps)
                ps = &(*ps)->prev;
            *ps = s, s->prev = NULL;
            si->data_size = si->sh_size;
        }
    }
#if 0
    for (i = 1; i < nbs; ++i) {
        Section *s = s1->sections[sec_order[i]];
        int type = s->sh_type;
        int flags = s->sh_flags;
        printf("section %-16s %-10s %p %04x %s,%s,%s\n",
            s->name,
            type == SHT_PROGBITS ? "progbits" :
            type == SHT_INIT_ARRAY ? "initarr" :
            type == SHT_FINI_ARRAY ? "finiarr" :
            type == SHT_NOBITS ? "nobits" :
            type == SHT_SYMTAB ? "symtab" :
            type == SHT_STRTAB ? "strtab" :
            type == SHT_RELX ? "rel" : "???",
            s->sh_addr,
            (unsigned)s->data_offset,
            flags & SHF_ALLOC ? "alloc" : "",
            flags & SHF_WRITE ? "write" : "",
            flags & SHF_EXECINSTR ? "exec" : ""
            );
        fflush(stdout);
    }
    s1->verbose = 2;
#endif
    mcc_free(sec_order);
    return 0;
}

static int pe_check_symbols(struct pe_info *pe)
{
    int sym_end;
    int ret = 0;
    MCCState *s1 = pe->s1;

    pe_align_section(text_section, 8);

    sym_end = symtab_section->data_offset / sizeof(ElfW(Sym));
    for (int sym_index = 1; sym_index < sym_end; ++sym_index) {
        ElfW(Sym) *sym = (ElfW(Sym) *)symtab_section->data + sym_index;
        if (sym->st_shndx == SHN_UNDEF) {
            const char *name = (char*)symtab_section->link->data + sym->st_name;
            unsigned type = ELFW(ST_TYPE)(sym->st_info);
            int imp_sym = 0;
            struct import_symbol *is;

            int _imp_, n;
            char buffer[200];
            const char *s, *p;

            n = _imp_ = 0;
            if (sym->st_other & ST_PE_IMPORT)
                _imp_ = 1;
            do {
                s = pe_export_name(s1, sym);
                if (n) {
                    if (sym->st_other & ST_PE_STDCALL) {
                        p = strrchr(s, '@');
                        if (!p || s[0] != '_')
                            break;
                        strcpy(buffer, s+1)[p-s-1] = 0, s = buffer;
                    } else if (s[0] != '_') {
                        buffer[0] = '_', strcpy(buffer + 1, s), s = buffer;
                    } else if (0 == memcmp(s, "_imp__", 6)) {
                        s += 6, _imp_ = 1;
                    } else if (0 == memcmp(s, "__imp_", 6)) {
                        s += 6, _imp_ = 1;
                    } else {
                        break;
                    }
                }
                imp_sym = find_elf_sym(s1->dynsymtab_section, s);
            } while (0 == imp_sym && ++n < 2);

            if (0 == imp_sym)
                continue;

            is = pe_add_import(pe, imp_sym);

            if (type == STT_FUNC
                || (type == STT_NOTYPE && 0 == _imp_)) {
                unsigned offset = is->thk_offset;
                if (offset) {
                } else {
                    unsigned char *p;

                    snprintf(buffer, sizeof(buffer), "IAT.%s", name);
                    is->iat_index = put_elf_sym(
                        symtab_section, 0, sizeof(DWORD),
                        ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT),
                        0, SHN_UNDEF, buffer);

                    offset = text_section->data_offset;
                    is->thk_offset = offset;

#ifdef MCC_TARGET_ARM
                    p = section_ptr_add(text_section, 8+4);
                    write32le(p + 0, 0xE59FC000);
                    write32le(p + 4, 0xE59CF000);
                    put_elf_reloc(symtab_section, text_section,
                        offset + 8, R_XXX_THUNKFIX, is->iat_index);
#elif defined(MCC_TARGET_ARM64)
                    p = section_ptr_add(text_section, 24);
                    write32le(p + 0, 0x58000090);
                    write32le(p + 4, 0xf9400210);
                    write32le(p + 8, 0xd61f0200);
                    write32le(p + 12, 0xd503201f);
                    put_elf_reloc(symtab_section, text_section,
                        offset + 16, R_XXX_THUNKFIX, is->iat_index);
#else
                    p = section_ptr_add(text_section, 8);
                    write16le(p, 0x25FF);
#ifdef MCC_TARGET_X86_64
                    write32le(p + 2, (DWORD)-4);
#endif
                    put_elf_reloc(symtab_section, text_section, 
                        offset + 2, R_XXX_THUNKFIX, is->iat_index);
#endif
                }
                sym = (ElfW(Sym) *)symtab_section->data + sym_index;
                sym->st_value = offset;
                sym->st_shndx = text_section->sh_num;
                sym->st_other &= ~ST_PE_EXPORT;

            } else {
                if (0 == _imp_)
                    ret = mcc_error_noabort("symbol '%s' is missing __declspec(dllimport)", name);
                sym->st_value = is->iat_index;
                is->iat_index = sym_index;
            }

        } else if (s1->rdynamic
                   && ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
            sym->st_other |= ST_PE_EXPORT;
        }
    }
    return ret;
}

#if PE_PRINT_SECTIONS
static void pe_print_section(FILE * f, Section * s)
{
    BYTE *p, *e, b;
    int i, n, l, m;
    p = s->data;
    e = s->data + s->data_offset;
    l = e - p;

    fprintf(f, "section  \"%s\"", s->name);
    if (s->link)
        fprintf(f, "\nlink     \"%s\"", s->link->name);
    if (s->reloc)
        fprintf(f, "\nreloc    \"%s\"", s->reloc->name);
    fprintf(f, "\nv_addr   %08X", (unsigned)s->sh_addr);
    fprintf(f, "\ncontents %08X", (unsigned)l);
    fprintf(f, "\n\n");

    if (s->sh_type == SHT_NOBITS)
        return;

    if (0 == l)
        return;

    if (s->sh_type == SHT_SYMTAB)
        m = sizeof(ElfW(Sym));
    else if (s->sh_type == SHT_RELX)
        m = sizeof(ElfW_Rel);
    else
        m = 16;

    fprintf(f, "%-8s", "offset");
    for (i = 0; i < m; ++i)
        fprintf(f, " %02x", i);
    n = 56;

    if (s->sh_type == SHT_SYMTAB || s->sh_type == SHT_RELX) {
        const char *fields1[] = {
            "name",
            "value",
            "size",
            "bind",
            "type",
            "other",
            "shndx",
            NULL
        };

        const char *fields2[] = {
            "offs",
            "type",
            "symb",
            NULL
        };

        const char **p;

        if (s->sh_type == SHT_SYMTAB)
            p = fields1, n = 106;
        else
            p = fields2, n = 58;

        for (i = 0; p[i]; ++i)
            fprintf(f, "%6s", p[i]);
        fprintf(f, "  symbol");
    }

    fprintf(f, "\n");
    for (i = 0; i < n; ++i)
        fprintf(f, "-");
    fprintf(f, "\n");

    for (i = 0; i < l;)
    {
        fprintf(f, "%08X", i);
        for (n = 0; n < m; ++n) {
            if (n + i < l)
                fprintf(f, " %02X", p[i + n]);
            else
                fprintf(f, "   ");
        }

        if (s->sh_type == SHT_SYMTAB) {
            ElfW(Sym) *sym = (ElfW(Sym) *) (p + i);
            const char *name = s->link->data + sym->st_name;
            fprintf(f, "  %04X  %04X  %04X   %02X    %02X    %02X   %04X  \"%s\"",
                    (unsigned)sym->st_name,
                    (unsigned)sym->st_value,
                    (unsigned)sym->st_size,
                    (unsigned)ELFW(ST_BIND)(sym->st_info),
                    (unsigned)ELFW(ST_TYPE)(sym->st_info),
                    (unsigned)sym->st_other,
                    (unsigned)sym->st_shndx,
                    name);

        } else if (s->sh_type == SHT_RELX) {
            ElfW_Rel *rel = (ElfW_Rel *) (p + i);
            ElfW(Sym) *sym =
                (ElfW(Sym) *) s->link->data + ELFW(R_SYM)(rel->r_info);
            const char *name = s->link->link->data + sym->st_name;
            fprintf(f, "  %04X   %02X   %04X  \"%s\"",
                    (unsigned)rel->r_offset,
                    (unsigned)ELFW(R_TYPE)(rel->r_info),
                    (unsigned)ELFW(R_SYM)(rel->r_info),
                    name);
        } else {
            fprintf(f, "   ");
            for (n = 0; n < m; ++n) {
                if (n + i < l) {
                    b = p[i + n];
                    if (b < 32 || b >= 127)
                        b = '.';
                    fprintf(f, "%c", b);
                }
            }
        }
        i += m;
        fprintf(f, "\n");
    }
    fprintf(f, "\n\n");
}

static void pe_print_sections(MCCState *s1, const char *fname)
{
    Section *s;
    FILE *f;
    int i;
    f = fopen(fname, "w");
    for (i = 1; i < s1->nb_sections; ++i) {
        s = s1->sections[i];
        pe_print_section(f, s);
    }
    pe_print_section(f, s1->dynsymtab_section);
    fclose(f);
}
#endif


ST_FUNC int pe_putimport(MCCState *s1, int dllindex, const char *name, addr_t value)
{
    return set_elf_sym(
        s1->dynsymtab_section,
        value,
        dllindex,
        ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
        0,
        value ? SHN_ABS : SHN_UNDEF,
        name
        );
}

static int read_mem(int fd, unsigned offset, void *buffer, unsigned len)
{
    lseek(fd, offset, SEEK_SET);
    return len == read(fd, buffer, len);
}


static int get_dllexports(int fd, char **pp)
{
    int i, k, l, n, n0, ret;
    char *p;

    IMAGE_SECTION_HEADER ish;
    IMAGE_EXPORT_DIRECTORY ied;
    IMAGE_DOS_HEADER dh;
    IMAGE_FILE_HEADER ih;
    DWORD sig, ref, addr;
    DWORD *namep = NULL, p0 = 0, p1;

    int pef_hdroffset, opt_hdroffset, sec_hdroffset;

    n = n0 = 0;
    p = NULL;
    ret = 1;
    if (!read_mem(fd, 0, &dh, sizeof dh))
        goto the_end;
    if (!read_mem(fd, dh.e_lfanew, &sig, sizeof sig))
        goto the_end;
    if (sig != 0x00004550)
        goto the_end;
    pef_hdroffset = dh.e_lfanew + sizeof sig;
    if (!read_mem(fd, pef_hdroffset, &ih, sizeof ih))
        goto the_end;
    opt_hdroffset = pef_hdroffset + sizeof ih;
    if (ih.Machine == 0x014C) {
        IMAGE_OPTIONAL_HEADER32 oh;
        sec_hdroffset = opt_hdroffset + sizeof oh;
        if (!read_mem(fd, opt_hdroffset, &oh, sizeof oh))
            goto the_end;
        if (IMAGE_DIRECTORY_ENTRY_EXPORT >= oh.NumberOfRvaAndSizes)
            goto the_end_0;
        addr = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    } else if (ih.Machine == 0x8664 || ih.Machine == IMAGE_FILE_MACHINE_ARM64) {
        IMAGE_OPTIONAL_HEADER64 oh;
        sec_hdroffset = opt_hdroffset + sizeof oh;
        if (!read_mem(fd, opt_hdroffset, &oh, sizeof oh))
            goto the_end;
        if (IMAGE_DIRECTORY_ENTRY_EXPORT >= oh.NumberOfRvaAndSizes)
            goto the_end_0;
        addr = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    } else
        goto the_end;

    for (i = 0; i < ih.NumberOfSections; ++i) {
        if (!read_mem(fd, sec_hdroffset + i * sizeof ish, &ish, sizeof ish))
            goto the_end;
        if (addr >= ish.VirtualAddress && addr < ish.VirtualAddress + ish.SizeOfRawData)
            goto found;
    }
    goto the_end_0;
found:
    ref = ish.VirtualAddress - ish.PointerToRawData;
    if (!read_mem(fd, addr - ref, &ied, sizeof ied))
        goto the_end;
    k = ied.NumberOfNames;
    if (k) {
        namep = mcc_malloc(l = k * sizeof *namep);
        if (!read_mem(fd, ied.AddressOfNames - ref, namep, l))
            goto the_end;
        for (i = l = 0; i < k; ++i) {
            p1 = namep[i] - ref;
            if (p1 != p0)
                lseek(fd, p0 = p1, SEEK_SET), l = 0;
            do {
                if (0 == l) {
                    if (n + 1000 >= n0)
                        p = mcc_realloc(p, n0 += 1000);
                    if ((l = read(fd, p + n, 1000 - 1)) <= 0)
                        goto the_end;
                }
                --l, ++p0;
            } while (p[n++]);
        }
        p[n] = 0;
    }
the_end_0:
    ret = 0;
the_end:
    mcc_free(namep);
    if (ret && p)
        mcc_free(p), p = NULL;
    *pp = p;
    return ret;
}


static int pe_load_res(MCCState *s1, int fd)
{
    struct pe_rsrc_header hdr;
    Section *rsrc_section;
    int ret = -1, sym_index;
    BYTE *ptr;
    unsigned offs;

    if (!read_mem(fd, 0, &hdr, sizeof hdr))
        goto quit;

    if (hdr.filehdr.Machine != IMAGE_FILE_MACHINE
        || hdr.filehdr.NumberOfSections != 1
        || strcmp((char*)hdr.sectionhdr.Name, ".rsrc") != 0)
        goto quit;

    rsrc_section = new_section(s1, ".rsrc", SHT_PROGBITS, SHF_ALLOC);
    ptr = section_ptr_add(rsrc_section, hdr.sectionhdr.SizeOfRawData);
    offs = hdr.sectionhdr.PointerToRawData;
    if (!read_mem(fd, offs, ptr, hdr.sectionhdr.SizeOfRawData))
        goto quit;
    offs = hdr.sectionhdr.PointerToRelocations;
    sym_index = put_elf_sym(symtab_section, 0, 0, 0, 0, rsrc_section->sh_num, ".rsrc");
    for (int i = 0; i < hdr.sectionhdr.NumberOfRelocations; ++i) {
        struct pe_rsrc_reloc rel;
        if (!read_mem(fd, offs, &rel, sizeof rel))
            goto quit;
        if (rel.type != RSRC_RELTYPE)
            goto quit;
        put_elf_reloc(symtab_section, rsrc_section,
            rel.offset, R_XXX_RELATIVE, sym_index);
        offs += sizeof rel;
    }
    ret = 0;
quit:
    return ret;
}


static char *trimfront(char *p)
{
    while ((unsigned char)*p <= ' ' && *p && *p != '\n')
	++p;
    return p;
}


static char *get_token(char **s, char *f)
{
    char *p = *s, *e;
    p = e = trimfront(p);
    while ((unsigned char)*e > ' ')
        ++e;
    *s = trimfront(e);
    *f = **s; *e = 0;
    return p;
}

static int pe_load_def(MCCState *s1, int fd)
{
    int state = 0, ret = -1, dllindex = 0, ord;
    char dllname[80], *buf, *line, *p, *x, next;

    buf = mcc_load_text(fd);
    if (!buf)
        return ret;

    for (line = buf;; ++line)  {
        p = get_token(&line, &next);
        if (!(*p && *p != ';'))
            goto skip;
        switch (state) {
        case 0:
            if (0 != stricmp(p, "LIBRARY") || next == '\n')
                goto quit;
            pstrcpy(dllname, sizeof dllname, get_token(&line, &next));
            ++state;
            break;
        case 1:
            if (0 != stricmp(p, "EXPORTS"))
                goto quit;
            ++state;
            break;
        case 2:
            dllindex = mcc_add_dllref(s1, dllname, 0)->index;
            ++state;
        default:
            ord = 0;
            if (next == '@') {
                x = get_token(&line, &next);
                ord = (int)strtol(x + 1, &x, 10);
            }
            pe_putimport(s1, dllindex, p, ord);
            break;
        }
skip:
        while ((unsigned char)next > ' ')
            get_token(&line, &next);
        if (next != '\n')
            break;
    }
    ret = 0;
quit:
    mcc_free(buf);
    return ret;
}


static int pe_load_dll(MCCState *s1, int fd, const char *filename)
{
    char *p, *q;
    DLLReference *ref = mcc_add_dllref(s1, filename, 0);
    if (ref->found)
        return 0;
    if (get_dllexports(fd, &p))
        return -1;
    if (p) {
        for (q = p; *q; q += 1 + strlen(q))
            pe_putimport(s1, ref->index, q, 0);
        mcc_free(p);
    }
    return 0;
}

ST_FUNC int pe_load_file(struct MCCState *s1, int fd, const char *filename)
{
    int ret = -1;
    char buf[10];
    if (0 == strcmp(mcc_fileextension(filename), ".def"))
        ret = pe_load_def(s1, fd);
    else if (pe_load_res(s1, fd) == 0)
        ret = 0;
    else if (read_mem(fd, 0, buf, 4) && 0 == memcmp(buf, "MZ", 2))
        ret = pe_load_dll(s1, fd, filename);
    return ret;
}

PUB_FUNC int mcc_get_dllexports(const char *filename, char **pp)
{
    int ret, fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        return -1;
    ret = get_dllexports(fd, pp);
    close(fd);
    return ret;
}

#ifdef MCC_TARGET_X86_64
static unsigned pe_add_unwind_info(MCCState *s1)
{
    if (NULL == s1->uw_pdata) {
        s1->uw_pdata = find_section(s1, ".pdata");
        s1->uw_pdata->sh_addralign = 4;
    }
    if (0 == s1->uw_sym)
        s1->uw_sym = put_elf_sym(symtab_section, 0, 0, 0, 0, text_section->sh_num, ".uw_base");
    if (0 == s1->uw_offs) {
        static const unsigned char uw_info[] = {
            0x01,
            0x04,
            0x02,
            0x05,
            0x04, 0x03,
            0x01, 0x50,
        };

        Section *s = text_section;
        unsigned char *p;

        section_ptr_add(s, -s->data_offset & 3);
        s1->uw_offs = s->data_offset;
        p = section_ptr_add(s, sizeof uw_info);
        memcpy(p, uw_info, sizeof uw_info);
    }

    return s1->uw_offs;
}

ST_FUNC void pe_add_unwind_data(unsigned start, unsigned end, unsigned stack)
{
    MCCState *s1 = mcc_state;
    Section *pd;
    unsigned o, n, d;
    struct   {
      DWORD BeginAddress;
      DWORD EndAddress;
      DWORD UnwindData;
    } *p;

    d = pe_add_unwind_info(s1);
    pd = s1->uw_pdata;
    o = pd->data_offset;
    p = section_ptr_add(pd, sizeof *p);

    p->BeginAddress = start;
    p->EndAddress = end;
    p->UnwindData = d;

    for (n = o + sizeof *p; o < n; o += sizeof p->BeginAddress)
        put_elf_reloc(symtab_section, pd, o, R_XXX_RELATIVE, s1->uw_sym);
}

#elif defined(MCC_TARGET_ARM64)
static Section *pe_add_unwind_info(MCCState *s1)
{
    Section *s;

    if (NULL == s1->uw_pdata) {
        s1->uw_pdata = find_section(s1, ".pdata");
        s1->uw_pdata->sh_addralign = 4;
    }
    s = find_section(s1, ".xdata");
    s->sh_addralign = 4;
    if (0 == s1->uw_sym)
        s1->uw_sym = put_elf_sym(symtab_section, 0, 0, 0, 0,
                                  text_section->sh_num, ".uw_text_base");
    if (0 == s1->uw_xsym)
        s1->uw_xsym = put_elf_sym(symtab_section, 0, 0, 0, 0,
                                  s->sh_num, ".uw_base");
    return s;
}

ST_FUNC void pe_add_unwind_data(unsigned start, unsigned end, unsigned stack)
{
    MCCState *s1 = mcc_state;
    Section *pd, *xd;
    unsigned o, d, code_bytes, func_len;
    unsigned char *q;
    uint32_t header;
    struct   {
        DWORD BeginAddress;
        DWORD UnwindData;
    } *p;

    int epilog;

    xd = pe_add_unwind_info(s1);
    pd = s1->uw_pdata;

    func_len = (end - start) >> 2;
    code_bytes = 0;
    epilog = code_bytes;
    code_bytes += 3;
    code_bytes = (code_bytes + 3) & ~3;

    section_ptr_add(xd, -xd->data_offset & 3);
    d = xd->data_offset;
    q = section_ptr_add(xd, 4 + code_bytes);

    header = (func_len & 0x3ffff)
        | 1 << 21
        | (epilog & 0x1F) << 22
        | (code_bytes >> 2) << 27
        ;
    write32le(q, header);
    q += 4;
    *q++ = 0xE1;
    *q++ = 0x9B;
    *q++ = 0xE4;
    while ((unsigned)(q - (xd->data + d + 4)) < code_bytes)
        *q++ = 0xE3;

    o = pd->data_offset;
    p = section_ptr_add(pd, sizeof *p);
    p->BeginAddress = start;
    p->UnwindData = d;
    put_elf_reloc(symtab_section, pd, o, R_XXX_RELATIVE, s1->uw_sym);
    put_elf_reloc(symtab_section, pd, o + 4, R_XXX_RELATIVE, s1->uw_xsym);
}
#endif
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
#define PE_STDSYM(n,s) n
#else
#define PE_STDSYM(n,s) "_" n s
#endif

static void pe_add_runtime(MCCState *s1, struct pe_info *pe)
{
    const char *start_symbol;
    int pe_type;

    if (MCC_OUTPUT_DLL == s1->output_type) {
        pe_type = PE_DLL;
        start_symbol = PE_STDSYM("__dllstart","@12");
    } else {
        const char *run_symbol;
        if (find_elf_sym(symtab_section, PE_STDSYM("WinMain","@16"))) {
            start_symbol = "__winstart";
            run_symbol = "__runwinmain";
            pe_type = PE_GUI;
        } else if (find_elf_sym(symtab_section, PE_STDSYM("wWinMain","@16"))) {
            start_symbol = "__wwinstart";
            run_symbol = "__runwwinmain";
            pe_type = PE_GUI;
        } else if (find_elf_sym(symtab_section, "wmain")) {
            start_symbol = "__wstart";
            run_symbol = "__runwmain";
            pe_type = PE_EXE;
        } else {
            start_symbol =  "__start";
            run_symbol = "__runmain";
            pe_type = PE_EXE;
            if (s1->pe_subsystem == 2)
                pe_type = PE_GUI;
        }

        if (MCC_OUTPUT_MEMORY == s1->output_type && !s1->nostdlib)
            start_symbol = run_symbol;
    }
    if (s1->elf_entryname) {
        pe->start_symbol = start_symbol = s1->elf_entryname;
    } else {
        pe->start_symbol = start_symbol + 1;
        if (!s1->leading_underscore || strchr(start_symbol, '@'))
            ++start_symbol;
    }

#ifdef CONFIG_MCC_BACKTRACE
    if (s1->do_backtrace) {
#ifdef CONFIG_MCC_BCHECK
        if (s1->do_bounds_check && s1->output_type != MCC_OUTPUT_DLL)
            mcc_add_support(s1, "bcheck.o");
#endif
        if (s1->output_type == MCC_OUTPUT_EXE)
            mcc_add_support(s1, "bt-exe.o");
        if (s1->output_type == MCC_OUTPUT_DLL)
            mcc_add_support(s1, "bt-dll.o");
        if (s1->output_type != MCC_OUTPUT_DLL)
            mcc_add_support(s1, "bt-log.o");
        mcc_add_btstub(s1);
    }
#endif

#ifdef MCC_IS_NATIVE
    if (MCC_OUTPUT_MEMORY != s1->output_type || s1->run_main)
#endif
    set_global_sym(s1, start_symbol, NULL, 0);

    if (0 == s1->nostdlib) {
        static const char * const libs[] = {
            "msvcrt", "kernel32", "", "user32", "gdi32", NULL
        };
        const char * const *pp, *p;
        if (MCC_LIBMCC1[0])
            mcc_add_support(s1, MCC_LIBMCC1);
        s1->static_link = 0;
        for (pp = libs; 0 != (p = *pp); ++pp) {
            if (*p)
                mcc_add_library(s1, p);
            else if (PE_DLL != pe_type && PE_GUI != pe_type)
                break;
        }
    }

    if (MCC_OUTPUT_DLL == s1->output_type)
        s1->output_type = MCC_OUTPUT_EXE;
    if (MCC_OUTPUT_MEMORY == s1->output_type)
        pe_type = PE_RUN;
    pe->type = pe_type;
}

ST_FUNC int pe_setsubsy(MCCState *s1, const char *arg)
{
    static const struct subsy { const char* p; int v; } x[] = {
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
        { "native", 1 },
        { "gui", 2 },
        { "windows", 2 },
        { "console", 3 },
        { "posix", 7 },
        { "efiapp", 10 },
        { "efiboot", 11 },
        { "efiruntime", 12 },
        { "efirom", 13 },
#elif defined(MCC_TARGET_ARM)
        { "wince", 9 },
#endif
        { 0, -1 }};
    const struct subsy *y;
    for (y = x;; ++y) {
        if (!y->p)
            return -1;
        if (0 == strcmp(y->p, arg)) {
            s1->pe_subsystem = y->v;
            return 0;
        }
    }
}

static void pe_set_options(MCCState * s1, struct pe_info *pe)
{
    if (PE_DLL == pe->type) {
#if defined(MCC_TARGET_ARM64)
        pe->imagebase = 0x180000000ULL;
#else
        pe->imagebase = 0x10000000;
#endif
    } else {
#if defined(MCC_TARGET_ARM)
        pe->imagebase = 0x00010000;
#elif defined(MCC_TARGET_ARM64)
        pe->imagebase = 0x140000000ULL;
#else
        pe->imagebase = 0x00400000;
#endif
    }

#if defined(MCC_TARGET_ARM)
    pe->subsystem = 9;
#else
    if (PE_DLL == pe->type || PE_GUI == pe->type)
        pe->subsystem = 2;
    else
        pe->subsystem = 3;
#endif
    if (s1->pe_subsystem != 0)
        pe->subsystem = s1->pe_subsystem;

    if (pe->subsystem == 1) {
        pe->section_align = 0x20;
        pe->file_align = 0x20;
    } else {
        pe->section_align = 0x1000;
        pe->file_align = 0x200;
    }

    if (s1->section_align != 0)
        pe->section_align = s1->section_align;
    if (s1->pe_file_align != 0)
        pe->file_align = s1->pe_file_align;

    if ((pe->subsystem >= 10) && (pe->subsystem <= 12))
        pe->imagebase = 0;

    if (s1->has_text_addr)
        pe->imagebase = s1->text_addr;
}

ST_FUNC int pe_output_file(MCCState *s1, const char *filename)
{
    struct pe_info pe;

    memset(&pe, 0, sizeof pe);
    pe.filename = filename;
    pe.s1 = s1;
    s1->filetype = 0;

#ifdef CONFIG_MCC_BCHECK
    mcc_add_bcheck(s1);
#endif
    mcc_add_pragma_libs(s1);
    pe_add_runtime(s1, &pe);
    resolve_common_syms(s1);
    pe_set_options(s1, &pe);
    pe_check_symbols(&pe);
    if (s1->nb_errors)
        goto done;
    if (filename) {
        pe_assign_addresses(&pe);
        relocate_syms(s1, s1->symtab, 0);
        if (s1->nb_errors)
            goto done;
        s1->pe_imagebase = pe.imagebase;
        relocate_sections(s1);
        pe.start_addr = (DWORD)
            (get_sym_addr(s1, pe.start_symbol, 1, 1) - pe.imagebase);
        if (s1->nb_errors)
            goto done;
        pe_write(&pe);
    } else {
#ifdef MCC_IS_NATIVE
        pe.thunk = data_section;
        pe_build_imports(&pe);
        s1->run_main = pe.start_symbol;
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
        s1->uw_pdata = find_section(s1, ".pdata");
#endif
#endif
    }
done:
    dynarray_reset(&pe.sec_info, &pe.sec_count);
    pe_free_imports(&pe);
#if PE_PRINT_SECTIONS
    if (g_debug & 8)
        pe_print_sections(s1, "mcc.log");
#endif
    return s1->nb_errors ? -1 : 0;
}

