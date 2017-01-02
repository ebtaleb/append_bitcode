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

/* Get the next load command from the current one */
#define NEXTCMD(cmd) (struct load_command*)((char*)(cmd) + (cmd)->cmdsize)

/* Iterate through all load commands */
#define ITERCMDS(i, cmd, cmds, ncmds) for(i = 0, cmd = (cmds); i < (ncmds); i++, cmd = NEXTCMD(cmd))

int inplace_flag = false;
int overwrite_flag = false;
int codesig_flag = 0;
int yes_flag = false;

static struct option long_options[] = {
	{"inplace",          no_argument, &inplace_flag,   true},
	{"overwrite",        no_argument, &overwrite_flag, true},
	{"strip-codesig",    no_argument, &codesig_flag,   1},
	{"no-strip-codesig", no_argument, &codesig_flag,   2},
	{"all-yes",          no_argument, &yes_flag,       true},
	{NULL,               0,           NULL,            0}
};

__attribute__((noreturn)) void usage(void) {
	printf("Usage: insert_segment data_path binary_path [new_binary_path]\n");

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

int replace_section(struct mach_header* mh, size_t filesize, const char* segname, const char* sectname, long data_size) {

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

    bool foundsect = false;

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
                             struct segment_command* seg = (struct segment_command*)cmd;
                             /*if(strncmp(seg->segname, segname, 16) == 0) {*/
                                 struct section* sects = (struct section*)&seg[1];
                                 for(uint32_t j = 0; j < seg->nsects; j++) {

                                     // fix up all the sections that follows
                                     if (foundsect) {
                                         printf("fixing VMA of %s from 0x%x to 0x%lx\n", sects[j].sectname, sects[j].addr, (sects[j].addr + data_size));
                                         sects[j].addr += data_size;
                                     }

                                     printf(" sectname %s.%s off %u size %u\n", sects[j].segname, sects[j].sectname, sects[j].offset, sects[j].size);
                                     if(strncmp(sects[j].sectname, sectname, 16) == 0) {
                                         foundsect = true;
                                         printf("found section %s,%s\n", segname, sectname);

                                         char stn[16] = "__bitcode";
                                         char sgn[16] = "__LLVM";
                                         memcpy(sects[j].sectname, stn, strlen(stn)+1);
                                         memcpy(sects[j].segname, sgn, strlen(sgn)+1);
                                         // leave VMA as it is
                                         sects[j].size = data_size;
                                         sects[j].offset = filesize;
                                         sects[j].align = 0;
                                         sects[j].reloff = 0;
                                         sects[j].nreloc = 0;
                                         sects[j].flags = 0;
                                         sects[j].reserved1 = 0;
                                         sects[j].reserved2 = 0;
                                     }
                                 }

                                // fixup the size of the segment
                                if(foundsect) {
                                    seg->vmsize += data_size;
                                    seg->filesize += data_size;
                                }
                             /*}*/
                             break;
                             }

            case LC_SEGMENT_64: {
                                struct segment_command_64* seg = (struct segment_command_64*)cmd;
                                /*if(strncmp(seg->segname, segname, 16) == 0) {*/
                                    struct section_64* sects = (struct section_64*)&seg[1];
                                    for(uint32_t j = 0; j < seg->nsects; j++) {

                                        // fix up all the sections that follows
                                        if (foundsect) {
                                            printf("fixing VMA of %s from 0x%llx to 0x%llx\n", sects[j].sectname, sects[j].addr, (sects[j].addr + data_size));
                                            sects[j].addr += data_size;
                                        }

                                        printf(" sectname %s.%s off %u size %llu\n", sects[j].segname, sects[j].sectname, sects[j].offset, sects[j].size);
                                        if(strncmp(sects[j].sectname, sectname, 16) == 0) {
                                            foundsect = true;
                                            printf("found section %s,%s\n", segname, sectname);

                                            char stn[16] = "__bitcode";
                                            char sgn[16] = "__LLVM";
                                            memcpy(sects[j].sectname, stn, strlen(stn)+1);
                                            memcpy(sects[j].segname, sgn, strlen(sgn)+1);
                                            // leave VMA as it is
                                            sects[j].size = data_size;
                                            sects[j].offset = filesize;
                                            sects[j].align = 0;
                                            sects[j].reloff = 0;
                                            sects[j].nreloc = 0;
                                            sects[j].flags = 0;
                                            sects[j].reserved1 = 0;
                                            sects[j].reserved2 = 0;
                                        }
                                    }

                                    // fixup the size of the segment
                                    if(foundsect) {
                                        seg->vmsize += data_size;
                                        seg->filesize += data_size;
                                    }
                                /*}*/
                                break;
                            }
        }
    }

    if(!foundsect) {
        fprintf(stderr, "Unable to find section %s,%s!\n", segname, sectname);
        return -1;
    }

    return 0;
}

off_t tell(int fd)
{
    return lseek(fd, 0, SEEK_CUR);
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

	if(argc < 3 || argc > 4) {
		usage();
	}

	const char *data_path = argv[1];
	const char *binary_path = argv[2];

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

	bool binary_path_was_malloced = false;
	if(!inplace_flag) {
		char *new_binary_path;
		if(argc == 4) {
			new_binary_path = (char *)argv[3];
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

    /* Get filesize for mmap */
    size_t filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    /* Get data to insert size */
    int data = open(data_path, O_RDONLY);

    if(!data) {
        printf("Couldn't open file %s\n", data_path);
        exit(1);
    }

    lseek(data, 0, SEEK_END);
    size_t dsize = tell(data);
    lseek(data, 0, SEEK_SET);

    printf("data size to insert : 0x%08zx\n", dsize);

    // determine start of new section
    size_t fsize = filesize;

    while ((fsize & 0xf) != 0) {
        fsize++;
    }

	int padding_bytes = fsize - filesize;
    printf("new section will be at 0x%zx, with 0x%x more bytes of padding\n", fsize, padding_bytes);

    /* Map the file */
    void* map = mmap(NULL, filesize, PROT_WRITE, MAP_SHARED, fd, 0);
    if(map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

	bool success = true;

	// append desired section

#define READ_BUFFER_SIZE 512

	unsigned char mybuffer[READ_BUFFER_SIZE];

	ssize_t bytesread = 1;
	int newsize = filesize;

    lseek(fd, filesize, SEEK_SET);

	char n[1] = "\0";
	for (int i = padding_bytes; i > 0; i--) {
		write(fd, n, 1);
		newsize++;
	}

	while ((bytesread = read(data, mybuffer, READ_BUFFER_SIZE)) > 0)
	{
		ssize_t written = write(fd, mybuffer, bytesread);
		newsize += written;
	}

	munmap(map, filesize);
    map = mmap(NULL, newsize, PROT_WRITE, MAP_SHARED, fd, 0);
    if(map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

	// replace a LC of our choice with the data we want
    // this is where the (un)fun starts : 
    // __debug_macinfo section choosen here is most of the time empty, so the one section preceding it and all the ones that follows it must have
    // their VM fixed (add the size of the section being modified to their starting VM) so that the VM layout is corehent
    //
    // in a nutshell, one wants to turn that jtool output :
    //
    // 	Mem: 0x0000008a9-0x0000008d9		__DWARF.__debug_aranges	(Normal)
	//  Mem: 0x000000000-0x00000000d		__LLVM.__bitcode
	//  Mem: 0x0000008d9-0x000000d24		__DWARF.__debug_line	(Normal)
	//  Mem: 0x000000d24-0x000000d47		__DWARF.__debug_loc	(Normal)
    //
    //  into:
    //
    // 	Mem: 0x0000008a9-0x0000008d9		__DWARF.__debug_aranges	(Normal)
	//  Mem: 0x0000008d9-0x0000008e6		__LLVM.__bitcode
	//  Mem: 0x0000008e6-0x000000d31		__DWARF.__debug_line	(Normal)
	//  Mem: 0x000000d31-0x000000d54		__DWARF.__debug_loc	(Normal)
    //
    int ret = replace_section(map, filesize + padding_bytes, "__DWARF", "__debug_macinfo", dsize);

    /* Clean up */
    munmap(map, newsize);
    close(fd);

	if(!success) {
		if(!inplace_flag) {
			unlink(binary_path);
		}
		exit(1);
	}

	if(binary_path_was_malloced) {
		free((void *)binary_path);
	}

    return ret;
}
