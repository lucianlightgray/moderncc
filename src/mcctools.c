#include "mcc.h"

#define ARFMAG "`\n"

typedef struct
{
	char ar_name[16];
	char ar_date[12];
	char ar_uid[6];
	char ar_gid[6];
	char ar_mode[8];
	char ar_size[10];
	char ar_fmag[2];
} ArHdr;

static unsigned long le2belong(unsigned long ul) { MCC_TRACE("enter\n");
	return ((ul & 0xFF0000) >> 8) + ((ul & 0xFF000000) >> 24) +
				 ((ul & 0xFF) << 24) + ((ul & 0xFF00) << 8);
}

static int ar_usage(int ret) { MCC_TRACE("enter\n");
	fprintf(stderr, "usage: mcc -ar [crstvx] lib [files]\n");
	fprintf(stderr, "create library ([abdiopN] not supported).\n");
	return ret;
}

ST_FUNC int mcc_tool_ar(int argc, char **argv) { MCC_TRACE("enter\n");
	static const ArHdr arhdr_init = {
			"/               ",
			"0           ",
			"0     ",
			"0     ",
			"0       ",
			"0         ",
			ARFMAG};

	ArHdr arhdr = arhdr_init;
	ArHdr arhdro = arhdr_init;

	FILE *fi, *fh = NULL, *fo = NULL;
	const char *created_file = NULL;
	ElfW(Ehdr) * ehdr;
	ElfW(Shdr) * shdr;
	ElfW(Sym) * sym;
	int fsize, i_lib, i_obj;
	char *buf, *shstr, *symtab, *strtab;
	int symtabsize = 0;
	char *anames = NULL;
	int *afpos = NULL;
	int istrlen, strpos = 0, fpos = 0, funccnt = 0, funcmax, hofs;
	char tfile[260], stmp[20];
	char *file, *name;
	int ret = 2;
	const char *ops_conflict = "habdiopN";
	int extract = 0;
	int table = 0;
	int verbose = 0;

	i_lib = 0;
	i_obj = 0;
	for (int i = 1; i < argc; i++) { MCC_TRACE("br\n");
		const char *a = argv[i];
		if (*a == '-' && strchr(a, '.'))
			{ MCC_TRACE("br\n"); ret = 1; }
		if ((*a == '-') || (i == 1 && !strchr(a, '.'))) { MCC_TRACE("br\n");
			if (strpbrk(a, ops_conflict))
				{ MCC_TRACE("br\n"); ret = 1; }
			if (strchr(a, 'x'))
				{ MCC_TRACE("br\n"); extract = 1; }
			if (strchr(a, 't'))
				{ MCC_TRACE("br\n"); table = 1; }
			if (strchr(a, 'v'))
				{ MCC_TRACE("br\n"); verbose = 1; }
		} else { MCC_TRACE("br\n");
			if (!i_lib)
				{ MCC_TRACE("br\n"); i_lib = i; }
			else if (!i_obj)
				{ MCC_TRACE("br\n"); i_obj = i; }
		}
	}

	if (!i_lib)
		{ MCC_TRACE("br\n"); ret = 1; }
	i_obj = i_obj ? i_obj : argc;

	if (ret == 1)
		{ MCC_TRACE("br\n"); return ar_usage(ret); }

	if (extract || table) { MCC_TRACE("br\n");
		if ((fh = fopen(argv[i_lib], "rb")) == NULL) { MCC_TRACE("br\n");
			fprintf(stderr, "mcc: ar: can't open file %s\n", argv[i_lib]);
			goto finish;
		}
		fread(stmp, 1, 8, fh);
		if (memcmp(stmp, ARMAG, 8)) { MCC_TRACE("br\n");
		no_ar:
			fprintf(stderr, "mcc: ar: not an ar archive %s\n", argv[i_lib]);
			goto finish;
		}
		while (fread(&arhdr, 1, sizeof(arhdr), fh) == sizeof(arhdr)) { MCC_TRACE("br\n");
			char *p, *e;

			if (memcmp(arhdr.ar_fmag, ARFMAG, 2))
				{ MCC_TRACE("br\n"); goto no_ar; }
			p = arhdr.ar_name;
			for (e = p + sizeof arhdr.ar_name; e > p && e[-1] == ' ';)
				{ MCC_TRACE("br\n"); e--; }
			*e = '\0';
			arhdr.ar_size[sizeof arhdr.ar_size - 1] = 0;
			fsize = atoi(arhdr.ar_size);
			buf = mcc_malloc(fsize + 1);
			fread(buf, fsize, 1, fh);
			if (strcmp(arhdr.ar_name, "/") && strcmp(arhdr.ar_name, "/SYM64/")) { MCC_TRACE("br\n");
				if (e > p && e[-1] == '/')
					{ MCC_TRACE("br\n"); e[-1] = '\0'; }
				if (table || verbose)
					{ MCC_TRACE("br\n"); printf("%s%s\n", extract ? "x - " : "", arhdr.ar_name); }
				if (extract) { MCC_TRACE("br\n");
					if ((fo = fopen(arhdr.ar_name, "wb")) == NULL) { MCC_TRACE("br\n");
						fprintf(stderr, "mcc: ar: can't create file %s\n",
										arhdr.ar_name);
						mcc_free(buf);
						goto finish;
					}
					fwrite(buf, fsize, 1, fo);
					fclose(fo);
				}
			}
			if (fsize & 1)
				{ MCC_TRACE("br\n"); fgetc(fh); }
			mcc_free(buf);
		}
		ret = 0;
	finish:
		if (fh)
			{ MCC_TRACE("br\n"); fclose(fh); }
		return ret;
	}

	if ((fh = fopen(argv[i_lib], "wb")) == NULL) { MCC_TRACE("br\n");
		fprintf(stderr, "mcc: ar: can't create file %s\n", argv[i_lib]);
		goto the_end;
	}
	created_file = argv[i_lib];

	snprintf(tfile, sizeof(tfile), "%s.tmp", argv[i_lib]);
	if ((fo = fopen(tfile, "wb+")) == NULL) { MCC_TRACE("br\n");
		fprintf(stderr, "mcc: ar: can't create temporary file %s\n", tfile);
		goto the_end;
	}

	funcmax = 250;
	afpos = mcc_realloc(NULL, funcmax * sizeof *afpos);
	memcpy(&arhdro.ar_mode, "100644", 6);

	while (i_obj < argc) { MCC_TRACE("br\n");
		if (*argv[i_obj] == '-') { MCC_TRACE("br\n");
			i_obj++;
			continue;
		}
		if ((fi = fopen(argv[i_obj], "rb")) == NULL) { MCC_TRACE("br\n");
			fprintf(stderr, "mcc: ar: can't open file %s \n", argv[i_obj]);
			goto the_end;
		}
		if (verbose)
			{ MCC_TRACE("br\n"); printf("a - %s\n", argv[i_obj]); }

		fseek(fi, 0, SEEK_END);
		fsize = ftell(fi);
		fseek(fi, 0, SEEK_SET);
		buf = mcc_malloc(fsize + 1);
		fread(buf, fsize, 1, fi);
		fclose(fi);

		ehdr = (ElfW(Ehdr) *)buf;
		if (ehdr->e_ident[4] != ELFCLASSW) { MCC_TRACE("br\n");
			fprintf(stderr, "mcc: ar: Unsupported Elf Class: %s\n", argv[i_obj]);
			goto the_end;
		}

		shdr = (ElfW(Shdr) *)(buf + ehdr->e_shoff + ehdr->e_shstrndx * ehdr->e_shentsize);
		shstr = (char *)(buf + shdr->sh_offset);
		symtab = strtab = NULL;
		for (int i = 0; i < ehdr->e_shnum; i++) { MCC_TRACE("br\n");
			shdr = (ElfW(Shdr) *)(buf + ehdr->e_shoff + i * ehdr->e_shentsize);
			if (!shdr->sh_offset)
				{ MCC_TRACE("br\n"); continue; }
			if (shdr->sh_type == SHT_SYMTAB) { MCC_TRACE("br\n");
				symtab = (char *)(buf + shdr->sh_offset);
				symtabsize = shdr->sh_size;
			}
			if (shdr->sh_type == SHT_STRTAB) { MCC_TRACE("br\n");
				if (!strcmp(shstr + shdr->sh_name, ".strtab")) { MCC_TRACE("br\n");
					strtab = (char *)(buf + shdr->sh_offset);
				}
			}
		}

		if (symtab && strtab) { MCC_TRACE("br\n");
			int nsym = symtabsize / sizeof(ElfW(Sym));
			for (int i = 1; i < nsym; i++) { MCC_TRACE("br\n");
				sym = (ElfW(Sym) *)(symtab + i * sizeof(ElfW(Sym)));
				if (sym->st_shndx &&
						(sym->st_info == 0x10 || sym->st_info == 0x11 || sym->st_info == 0x12 || sym->st_info == 0x20 || sym->st_info == 0x21 || sym->st_info == 0x22)) { MCC_TRACE("br\n");
					istrlen = strlen(strtab + sym->st_name) + 1;
					anames = mcc_realloc(anames, strpos + istrlen);
					strcpy(anames + strpos, strtab + sym->st_name);
					strpos += istrlen;
					if (++funccnt >= funcmax) { MCC_TRACE("br\n");
						funcmax += 250;
						afpos = mcc_realloc(afpos, funcmax * sizeof *afpos);
					}
					afpos[funccnt] = fpos;
				}
			}
		}

		file = argv[i_obj];
		for (name = strchr(file, 0);
				 name > file && name[-1] != '/' && name[-1] != '\\';
				 --name)
			;
		istrlen = strlen(name);
		if (istrlen >= sizeof(arhdro.ar_name))
			{ MCC_TRACE("br\n"); istrlen = sizeof(arhdro.ar_name) - 1; }
		memset(arhdro.ar_name, ' ', sizeof(arhdro.ar_name));
		memcpy(arhdro.ar_name, name, istrlen);
		arhdro.ar_name[istrlen] = '/';
		snprintf(stmp, sizeof(stmp), "%-10d", fsize);
		memcpy(&arhdro.ar_size, stmp, 10);
		fwrite(&arhdro, sizeof(arhdro), 1, fo);
		fwrite(buf, fsize, 1, fo);
		mcc_free(buf);
		i_obj++;
		fpos += (fsize + sizeof(arhdro));
		if (fpos & 1)
			{ MCC_TRACE("br\n"); fputc(0, fo), ++fpos; }
	}
	hofs = 8 + sizeof(arhdr) + strpos + (funccnt + 1) * sizeof(int);
	fpos = 0;
	if ((hofs & 1))
		{ MCC_TRACE("br\n"); hofs++, fpos = 1; }
	fwrite(ARMAG, 8, 1, fh);
	if (!funccnt) { MCC_TRACE("br\n");
		ret = 0;
		goto the_end;
	}
	snprintf(stmp, sizeof(stmp), "%-10d", (int)(strpos + (funccnt + 1) * sizeof(int)) + fpos);
	memcpy(&arhdr.ar_size, stmp, 10);
	fwrite(&arhdr, sizeof(arhdr), 1, fh);
	afpos[0] = le2belong(funccnt);
	for (int i = 1; i <= funccnt; i++)
		{ MCC_TRACE("br\n"); afpos[i] = le2belong(afpos[i] + hofs); }
	fwrite(afpos, (funccnt + 1) * sizeof(int), 1, fh);
	fwrite(anames, strpos, 1, fh);
	if (fpos)
		{ MCC_TRACE("br\n"); fwrite("", 1, 1, fh); }
	fseek(fo, 0, SEEK_END);
	fsize = ftell(fo);
	fseek(fo, 0, SEEK_SET);
	buf = mcc_malloc(fsize + 1);
	fread(buf, fsize, 1, fo);
	fwrite(buf, fsize, 1, fh);
	mcc_free(buf);
	ret = 0;
the_end:
	if (anames)
		{ MCC_TRACE("br\n"); mcc_free(anames); }
	if (afpos)
		{ MCC_TRACE("br\n"); mcc_free(afpos); }
	if (fh)
		{ MCC_TRACE("br\n"); fclose(fh); }
	if (created_file && ret != 0)
		{ MCC_TRACE("br\n"); remove(created_file); }
	if (fo)
		{ MCC_TRACE("br\n"); fclose(fo), remove(tfile); }
	return ret;
}

#ifdef MCC_TARGET_PE

ST_FUNC int mcc_tool_impdef(int argc, char **argv) { MCC_TRACE("enter\n");
	int ret, v, i;
	char infile[260];
	char outfile[260];

	const char *file;
	char *p, *q;
	FILE *fp, *op;
	char path[260];

	infile[0] = outfile[0] = 0;
	fp = op = NULL;
	ret = 1;
	p = NULL;
	v = 0;

	for (i = 1; i < argc; ++i) { MCC_TRACE("br\n");
		const char *a = argv[i];
		if ('-' == a[0]) { MCC_TRACE("br\n");
			if (0 == strcmp(a, "-v")) { MCC_TRACE("br\n");
				v = 1;
			} else if (0 == strcmp(a, "-o")) { MCC_TRACE("br\n");
				if (++i == argc)
					{ MCC_TRACE("br\n"); goto usage; }
				pstrcpy(outfile, sizeof outfile, argv[i]);
			} else
				{ MCC_TRACE("br\n"); goto usage; }
		} else if (0 == infile[0])
			{ MCC_TRACE("br\n"); pstrcpy(infile, sizeof infile, a); }
		else
			{ MCC_TRACE("br\n"); goto usage; }
	}

	if (0 == infile[0]) { MCC_TRACE("br\n");
	usage:
		fprintf(stderr,
						"usage: mcc -impdef library.dll [-v] [-o outputfile]\n"
						"create export definition file (.def) from dll\n");
		goto the_end;
	}

	if (0 == outfile[0]) { MCC_TRACE("br\n");
		pstrcpy(outfile, sizeof outfile, mcc_basename(infile));
		q = strrchr(outfile, '.');
		if (NULL == q)
			{ MCC_TRACE("br\n"); q = strchr(outfile, 0); }
		pstrcpy(q, sizeof outfile - (q - outfile), ".def");
	}

	file = infile;
	if (host_find_tool(file, ".dll", path, sizeof path))
		{ MCC_TRACE("br\n"); file = path; }
	ret = mcc_get_dllexports(file, &p);
	if (ret || !p) { MCC_TRACE("br\n");
		fprintf(stderr, "mcc: impdef: %s '%s'\n",
						ret == -1
								? "can't find file"
						: ret == 1
								? "can't read symbols"
						: ret == 0
								? "no symbols found in"
								: "unknown file type",
						file);
		ret = 1;
		goto the_end;
	}

	if (v)
		{ MCC_TRACE("br\n"); printf("-> %s\n", file); }

	op = fopen(outfile, "wb");
	if (NULL == op) { MCC_TRACE("br\n");
		fprintf(stderr, "mcc: impdef: could not create output file: %s\n", outfile);
		goto the_end;
	}

	fprintf(op, "LIBRARY %s\n\nEXPORTS\n", mcc_basename(file));
	for (q = p, i = 0; *q; ++i) { MCC_TRACE("br\n");
		fprintf(op, "%s\n", q);
		q += strlen(q) + 1;
	}

	if (v)
		{ MCC_TRACE("br\n"); printf("<- %s (%d symbol%s)\n", outfile, i, &"s"[i < 2]); }

	ret = 0;

the_end:
	if (p)
		{ MCC_TRACE("br\n"); mcc_free(p); }
	if (fp)
		{ MCC_TRACE("br\n"); fclose(fp); }
	if (op)
		{ MCC_TRACE("br\n"); fclose(op); }
	return ret;
}

#endif

#if !defined MCC_TARGET_I386 && !defined MCC_TARGET_X86_64

ST_FUNC int mcc_tool_cross(char **argv, int option) { MCC_TRACE("enter\n");
	fprintf(stderr, "mcc -m%d not implemented\n", option);
	return 1;
}

#else

ST_FUNC int mcc_tool_cross(char **argv, int target) { MCC_TRACE("enter\n");
	char program[4096];
	char *a0 = argv[0];
	int prefix = mcc_basename(a0) - a0;

	snprintf(program, sizeof program,
					 "%.*s%s"
#ifdef MCC_TARGET_PE
					 "-win32"
#endif
					 "-mcc" HOST_EXE_SUFFIX,
					 prefix, a0, target == 64 ? "x86_64" : "i386");

	if (strcmp(a0, program)) { MCC_TRACE("br\n");
		argv[0] = program;
		host_exec_replace(argv);
	}
	fprintf(stderr, "mcc: could not run '%s'\n", program);
	return 1;
}

#endif

#if MCC_HOST_WIN32
const int _CRT_glob = 1;
#ifndef _CRT_glob
const int _dowildcard = 1;
#endif
#endif

static char *escape_target_dep(const char *s) { MCC_TRACE("enter\n");
	char *res = mcc_malloc(strlen(s) * 2 + 1);
	int j;
	for (j = 0; *s; s++, j++) { MCC_TRACE("br\n");
		if (is_space(*s)) { MCC_TRACE("br\n");
			res[j++] = '\\';
		}
		res[j] = *s;
	}
	res[j] = '\0';
	return res;
}

ST_FUNC int gen_makedeps(MCCState *s1, const char *target, const char *filename) { MCC_TRACE("enter\n");
	FILE *depout;
	char buf[1024];
	char **escaped_targets;
	int num_targets;

	if (!filename) { MCC_TRACE("br\n");
		snprintf(buf, sizeof buf, "%.*s.d",
						 (int)(mcc_fileextension(target) - target), target);
		filename = buf;
	}

	if (s1->dep_target)
		{ MCC_TRACE("br\n"); target = s1->dep_target; }

	if (!strcmp(filename, "-"))
		{ MCC_TRACE("br\n"); depout = fdopen(1, "w"); }
	else
		{ MCC_TRACE("br\n"); depout = fopen(filename, "w"); }
	if (!depout)
		{ MCC_TRACE("br\n"); return mcc_error_noabort("could not open '%s'", filename); }
	if (s1->verbose)
		{ MCC_TRACE("br\n"); printf("<- %s\n", filename); }

	escaped_targets = mcc_malloc(s1->nb_target_deps * sizeof(*escaped_targets));
	num_targets = 0;
	for (int i = 0; i < s1->nb_target_deps; ++i) { MCC_TRACE("br\n");
		for (int k = 0; k < i; ++k)
			{ MCC_TRACE("br\n"); if (0 == strcmp(s1->target_deps[i], s1->target_deps[k]))
				{ MCC_TRACE("br\n"); goto next; } }
		escaped_targets[num_targets++] = escape_target_dep(s1->target_deps[i]);
	next:;
	}

	fprintf(depout, "%s:", target);
	for (int i = 0; i < num_targets; ++i)
		{ MCC_TRACE("br\n"); fprintf(depout, " \\\n  %s", escaped_targets[i]); }
	fprintf(depout, "\n");
	if (s1->gen_phony_deps) { MCC_TRACE("br\n");
		for (int i = 1; i < num_targets; ++i)
			{ MCC_TRACE("br\n"); fprintf(depout, "%s:\n", escaped_targets[i]); }
	}
	for (int i = 0; i < num_targets; ++i)
		{ MCC_TRACE("br\n"); mcc_free(escaped_targets[i]); }
	mcc_free(escaped_targets);
	fclose(depout);
	return 0;
}
