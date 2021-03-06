#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <copyfile.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* Get the next load command from the current one */
#define NEXTCMD(cmd) (struct load_command*)((char*)(cmd) + (cmd)->cmdsize)

/* Iterate through all load commands */
#define ITERCMDS(i, cmd, cmds, ncmds) for(i = 0, cmd = (cmds); i < (ncmds); i++, cmd = NEXTCMD(cmd))

#define MAX_SECTIONS 4

int inplace_flag = false;
int overwrite_flag = false;
int codesig_flag = 0;
int yes_flag = false;
char candidates[MAX_SECTIONS][16] = { {0} };

static struct option long_options[] = {
	{"inplace",          no_argument,       &inplace_flag,   true},
	{"overwrite",        no_argument,       &overwrite_flag, true},
	{"strip-codesig",    no_argument,       &codesig_flag,   1},
	{"no-strip-codesig", no_argument,       &codesig_flag,   2},
	{"all-yes",          no_argument,       &yes_flag,       true},
	{"candidates",       required_argument, NULL,            'c'},
	{NULL,               0,                 NULL,            0}
};

__attribute__((noreturn)) void usage(void) {
	printf("Usage: insert_segment data_path1 data_path2 binary_path [new_binary_path] [comma_separated_section_list]\n");

	printf("Option flags:");

	struct option *opt = long_options;
	while(opt->name != NULL) {
		printf(" --%s", opt->name);
		opt++;
	}

	printf("\n");

	exit(1);
}

__attribute__((format(printf, 1, 2))) bool ask(const char *format, ...) {
	char *question;
	asprintf(&question, "%s [y/n] ", format);

	va_list args;
	va_start(args, format);
	vprintf(question, args);
	va_end(args);

	free(question);

	while(true) {
		char *line = NULL;
		size_t size;
		if(yes_flag) {
			puts("y");
			line = "y";
		} else {
			getline(&line, &size, stdin);
		}

		switch(line[0]) {
			case 'y':
			case 'Y':
				return true;
				break;
			case 'n':
			case 'N':
				return false;
				break;
			default:
				printf("Please enter y or n: ");
		}
	}
}

int replace_section(struct mach_header* mh, size_t filesize, const char* segname, const char* sectname, long data_size, const char *sgn, const char *stn, int d2_opt) {

    bool is64bit = false;

    uint32_t i, ncmds;
    struct load_command* cmd, *cmds;

    /* Parse mach_header to get the first load command and the number of commands */
    if(mh->magic != MH_MAGIC) {
        if(mh->magic == MH_MAGIC_64) {
            is64bit = true;
            struct mach_header_64* mh64 = (struct mach_header_64*)mh;
            cmds = (struct load_command*)&mh64[1];
            ncmds = mh64->ncmds;
        }
        else {
            fprintf(stderr, "Invalid magic number: %08X\n", mh->magic);
            return -1;
        }
    }
    else {
        cmds = (struct load_command*)&mh[1];
        ncmds = mh->ncmds;
    }

    int foundsect = -1;
    uint32_t idx = 0;

    /* Iterate through the mach-o's load commands */
    ITERCMDS(i, cmd, cmds, ncmds) {
        /* Make sure we don't loop forever */
        if(cmd->cmdsize == 0) {
            break;
        }

        /* Make sure the load command is completely contained in the file */
        if((uintptr_t)cmd + cmd->cmdsize - (uintptr_t)mh > filesize) {
            break;
        }

        /* Process the load command */
        switch(cmd->cmd) {
            case LC_SEGMENT: {
                             uint32_t vm_max = 0;
                             struct segment_command* seg = (struct segment_command*)cmd;
                             /*if(strncmp(seg->segname, segname, 16) == 0) {*/
                                 struct section* sects = (struct section*)&seg[1];
                                 for(uint32_t j = 0; j < seg->nsects; j++) {

                                     printf(" sectname %s.%s off %u size %u\n", sects[j].segname, sects[j].sectname, sects[j].offset, sects[j].size);
                                     if (sects[j].addr + sects[j].size > 0) {
                                         vm_max = sects[j].addr + sects[j].size;
                                         idx = j;
                                     }

                                     if(strncmp(sects[j].sectname, sectname, 16) == 0) {
                                         foundsect = j;
                                         printf("found section %s,%s\n", segname, sectname);
                                     }
                                 }

                                // fixup the size of the segment
                                printf("max section is %s @ 0x%x\n", sects[idx].sectname, vm_max);
                                if(foundsect >= 0) {
                                    seg->vmsize += data_size;
                                    seg->filesize += data_size;

                                    memcpy(sects[foundsect].sectname, stn, strlen(stn)+1);
                                    memcpy(sects[foundsect].segname, sgn, strlen(sgn)+1);
                                    // leave VMA as it is
                                    if (d2_opt <= 0) 
                                        sects[foundsect].addr = vm_max;
                                    else
                                        sects[foundsect].addr = vm_max + d2_opt;
                                    sects[foundsect].size = data_size;
                                    sects[foundsect].offset = filesize;
                                    sects[foundsect].align = 0;
                                    sects[foundsect].reloff = 0;
                                    sects[foundsect].nreloc = 0;
                                    sects[foundsect].flags = 0;
                                    sects[foundsect].reserved1 = 0;
                                    sects[foundsect].reserved2 = 0;
                                }
                             /*}*/
                             break;
                             }

            case LC_SEGMENT_64: {
                                uint64_t vm_max = 0;
                                struct segment_command_64* seg = (struct segment_command_64*)cmd;
                                /*if(strncmp(seg->segname, segname, 16) == 0) {*/
                                    struct section_64* sects = (struct section_64*)&seg[1];
                                    for(uint32_t j = 0; j < seg->nsects; j++) {

                                        printf(" sectname %s.%s off %u addr 0x%llx size %llu\n", sects[j].segname, sects[j].sectname, sects[j].offset, sects[j].addr, sects[j].size);
                                        if (sects[j].addr + sects[j].size > 0) {
                                            vm_max = sects[j].addr + sects[j].size;
                                            idx = j;
                                        }

                                        if(strncmp(sects[j].sectname, sectname, 16) == 0) {
                                            foundsect = j;
                                            printf("found section %s,%s\n", segname, sectname);
                                        }
                                    }

                                    // fixup the size of the segment
                                    printf("max section is %s @ 0x%llx\n", sects[idx].sectname, vm_max);
                                    if(foundsect >= 0) {
                                        printf("wtf foundsect %d\n", foundsect);
                                        seg->vmsize += data_size;
                                        seg->filesize += data_size;

                                        memcpy(sects[foundsect].sectname, stn, strlen(stn)+1);
                                        memcpy(sects[foundsect].segname, sgn, strlen(sgn)+1);
                                        // leave VMA as it is
                                        if (d2_opt <= 0) 
                                            sects[foundsect].addr = vm_max;
                                        else
                                            sects[foundsect].addr = vm_max + d2_opt;
                                        sects[foundsect].size = data_size;
                                        sects[foundsect].offset = filesize;
                                        sects[foundsect].align = 0;
                                        sects[foundsect].reloff = 0;
                                        sects[foundsect].nreloc = 0;
                                        sects[foundsect].flags = 0;
                                        sects[foundsect].reserved1 = 0;
                                        sects[foundsect].reserved2 = 0;
                                    }

                                /*}*/
                                break;
                            }
        }
    }

    if(foundsect < 0) {
        fprintf(stderr, "Unable to find section %s,%s!\n", segname, sectname);
        return -1;
    }

    return foundsect;
}

off_t tell(int fd)
{
    return lseek(fd, 0, SEEK_CUR);
}

int getsize(const char *p) {
    struct stat st;
    stat(p, &st);
    return st.st_size;
}

off_t append_data(int fd, int data, int filesize) {

    uint32_t READ_BUFFER_SIZE = 512;

	unsigned char mybuffer[READ_BUFFER_SIZE];

	ssize_t bytesread = 0;
	int newsize = filesize;

    lseek(fd, filesize, SEEK_SET);

	char n = 0;
    while ((tell(fd) & 0xf) != 0) {
		write(fd, &n, 1);
		newsize++;
	}

	while ((bytesread = read(data, mybuffer, READ_BUFFER_SIZE)) > 0)
	{
		ssize_t written = write(fd, mybuffer, bytesread);
		newsize += written;
	}

    return newsize;
}

int main(int argc, const char *argv[]) {
	while(true) {
		int option_index = 0;

		int c = getopt_long(argc, (char *const *)argv, "", long_options, &option_index);

		if(c == -1) {
			break;
		}

		switch(c) {
			case 0:
				break;
            case 'c': {
                /*printf("option -c with value `%s'\n", optarg);*/
                char *pt;
                int i = 0;
                pt = strtok(optarg, ",");
                while (pt != NULL && i < MAX_SECTIONS) {
                    /*printf("got %s\n", pt);*/
                    strncpy(candidates[i], pt, strlen(pt));
                    pt = strtok(NULL, ",");
                    i++;
                }
                break;
            }
			case '?':
				usage();
				break;
			default:
				abort();
				break;
		}
	}

	argv = &argv[optind - 1];
	argc -= optind - 1;

	if(argc < 4 || argc > 5) {
		usage();
	}

	const char *data_path = argv[1];
	const char *data_path2 = argv[2];
	const char *binary_path = argv[3];

	struct stat s;

	if(stat(binary_path, &s) != 0) {
		perror(binary_path);
		exit(1);
	}

	if(data_path[0] != '@' && stat(data_path, &s) != 0) {
		if(!ask("The provided data path doesn't exist. Continue anyway?")) {
			exit(1);
		}
	}

	if(data_path2[0] != '@' && stat(data_path2, &s) != 0) {
		if(!ask("The provided data path doesn't exist. Continue anyway?")) {
			exit(1);
		}
	}

	bool binary_path_was_malloced = false;
	if(!inplace_flag) {
		char *new_binary_path;
		if(argc == 5) {
			new_binary_path = (char *)argv[4];
		} else {
			asprintf(&new_binary_path, "%s_patched", binary_path);
			binary_path_was_malloced = true;
		}

		if(!overwrite_flag && stat(new_binary_path, &s) == 0) {
			if(!ask("%s already exists. Overwrite it?", new_binary_path)) {
				exit(1);
			}
		}

		if(copyfile(binary_path, new_binary_path, NULL, COPYFILE_DATA | COPYFILE_UNLINK)) {
			printf("Failed to create %s\n", new_binary_path);
			exit(1);
		}

		binary_path = new_binary_path;
	}

    int fd = open(binary_path, O_RDWR);
    if(fd == -1) {
        printf("Couldn't open file %s\n", binary_path);
        return -1;
    }

    /* Get data to insert size */
    int data = open(data_path, O_RDONLY);
    if(!data) {
        printf("Couldn't open file %s\n", data_path);
        exit(1);
    }

    int data2 = open(data_path2, O_RDONLY);
    if(!data2) {
        printf("Couldn't open file %s\n", data_path2);
        exit(1);
    }

    /* Get filesize for mmap */
    size_t filesize = getsize(binary_path);
    size_t dsize = getsize(data_path);
    size_t dsize2 = getsize(data_path2);

    printf("data size to insert : 0x%08zx\n", dsize);

    // determine start of new section
    size_t fsize = filesize;

    while ((fsize & 0xf) != 0) {
        fsize++;
    }

	int padding_bytes = fsize - filesize;
    printf("new section will be at 0x%zx, with 0x%x more bytes of padding\n", fsize, padding_bytes);

    fsize = filesize + padding_bytes + dsize;

    while ((fsize & 0xf) != 0) {
        fsize++;
    }

	int padding_bytes2 = (fsize - (filesize + padding_bytes + dsize));

    /* Map the file */
    void* map = mmap(NULL, filesize, PROT_WRITE, MAP_SHARED, fd, 0);
    if(map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

	bool success = true;

	// append desired section
    int newsize = append_data(fd, data, filesize);
    newsize = append_data(fd, data2, newsize);

	munmap(map, filesize);
    map = mmap(NULL, newsize, PROT_WRITE, MAP_SHARED, fd, 0);
    if(map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

	// replace a LC of our choice with the data we want
    printf("data1 ofs : %zu @ 0x%lx, filesize = %ld, pad1 = %d\n", dsize, (filesize + padding_bytes), filesize, padding_bytes);
    int ret1 = replace_section(map, (filesize + padding_bytes), "__DWARF", "__debug_macinfo", dsize, "__LLVM", "__bitcode", -1);
    printf("data2 ofs : %zu @ 0x%lx, filesize = %ld, pad2 = %d\n", dsize2, (filesize + padding_bytes + dsize + padding_bytes2), filesize, padding_bytes2);
    int ret2 = replace_section(map, (filesize + padding_bytes + dsize + padding_bytes2), "__DWARF", "__apple_objc", dsize2, "__LLVM", "__cmdline", dsize);

    /* Clean up */
    munmap(map, newsize);
    close(fd);
    close(data);
    close(data2);

	if(!success) {
		if(!inplace_flag) {
			unlink(binary_path);
		}
		exit(1);
	}

	if(binary_path_was_malloced) {
		free((void *)binary_path);
	}

    return ret1 & ret2;
}
