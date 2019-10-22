//NAME: Lihang Liu, Zixuan Lu
//EMAIL: lliu0809@g.ucla.edu
//ID: 604972806

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>
#include "ext2_fs.h"

#define superBlockOffset 1024
#define superBlockLength 1024

char* fs_image_name;
int fd;
unsigned int bsize; //block_size
static struct ext2_super_block superblock;
static struct ext2_group_desc group;

int block_offset(int block_num) {
	return (superBlockOffset + (block_num - 1) * bsize);
}

void superblock_sum() {
	if (pread(fd, &superblock, superBlockLength, superBlockOffset) < 0) {
		fprintf(stderr, "pread superblock error.\n");
		exit(2);
	}

	if (superblock.s_magic != EXT2_SUPER_MAGIC) {
		fprintf(stderr, "magic number does not match.\n");
		exit(2);
	}

	bsize = EXT2_MIN_BLOCK_SIZE << superblock.s_log_block_size;

	printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", superblock.s_blocks_count,
			superblock.s_inodes_count, bsize, superblock.s_inode_size,
			superblock.s_blocks_per_group, superblock.s_inodes_per_group,
			superblock.s_first_ino);
}

void groupSummary() {
	int ret = pread(fd, &group, sizeof(struct ext2_group_desc),
	superBlockOffset + sizeof(struct ext2_super_block));
	if (ret < 0) {
		fprintf(stderr, "Reading group failed.\n");
		exit(2);
	}

	__u32 blocks = superblock.s_blocks_count;
	__u32 inodes = superblock.s_inodes_count;
	__u32 freeBlocks = group.bg_free_blocks_count;
	__u32 freeinodes = group.bg_free_inodes_count;
	__u32 blockBitmap = group.bg_block_bitmap;
	__u32 inodeBitmap = group.bg_inode_bitmap;
	__u32 firstBlock = group.bg_inode_table;

	fprintf(stdout, "GROUP,0,%d,%d,%d,%d,%d,%d,%d\n", blocks, inodes,
			freeBlocks, freeinodes, blockBitmap, inodeBitmap, firstBlock);
}

void free_block_entries() {
	__u8* bitmap = (__u8*)malloc(bsize);

	pread(fd, bitmap, bsize, block_offset(group.bg_block_bitmap));

	for (unsigned int i =0; i < superblock.s_blocks_count; i++) {
		__u8 byte = bitmap[i];
		for (int j = 0; j < 8; j++) {
			if (!(byte & 1)) {
				int current_block_num = 8*i+j+superblock.s_first_data_block;
				fprintf(stdout, "BFREE,%d\n",current_block_num );
			}
			byte >>= 1;
		}
	}
	free(bitmap);
}

void freeinode() {
	__u8* bitmap = (__u8*)malloc(bsize);

	int ret = pread(fd, bitmap, bsize, block_offset(group.bg_inode_bitmap));
	if(ret < 0) {
		fprintf(stderr,"pread failed.\n");
		exit(2);
	}

	unsigned int i;
	for(i = 0; i < superblock.s_inodes_count; i++) {
		__u8 byte = bitmap[i];
		int j;
		for(j = 0; j < 8; j++) {
			if(!(byte & 1)) {
				int p = (8 * i) + (j + 1);
				fprintf(stdout, "IFREE,%d\n", p);
			}
			byte >>= 1;
		}
	}
	free(bitmap);
}

void directoryEntry(__u32 inodeNum, __u32 blockNum, __u32 logical_offset) {
	struct ext2_dir_entry directoryEntry;
	long offset = block_offset(blockNum);
	__u32 i = 0;

	while (i < bsize) {
		pread(fd, &directoryEntry, sizeof(struct ext2_dir_entry), offset + i);
		if (directoryEntry.inode != 0) {
			fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n", inodeNum,
					i + logical_offset, directoryEntry.inode,
					directoryEntry.rec_len, directoryEntry.name_len,
					directoryEntry.name);
		}

		i += directoryEntry.rec_len;
	}

}

void directoryEntry_multi(__u32 inodeNum, __u32 blockNum, int level,
		__u32 logical_offset) {
	__u32 block;
	if (level > 0) {
		for (unsigned int i = 0; i < bsize / sizeof(__u32 ); i++) {
			pread(fd, &block, sizeof(__u32 ),
					block_offset(blockNum) + i * sizeof(__u32 ));
			if (block != 0) {
				__u32 new_offset;
				switch (level) {
				case 1:
					new_offset = bsize;
					break;
				case 2:
					new_offset = bsize * (bsize / sizeof(__u32 ));
					break;
				case 3:
					new_offset = bsize * (bsize / sizeof(__u32 ))
							* (bsize / sizeof(__u32 ));
					break;
				default:
					fprintf(stderr, "Unknown level %d\n", level);
					exit(2);
				}
				directoryEntry_multi(inodeNum, block, level - 1,
						logical_offset + new_offset * i);
			}
		}
	} else {
		directoryEntry(inodeNum, blockNum, logical_offset);
	}

}

void directory_driver(__u32 inodeNum, struct ext2_inode inode) {
	__u32 offset;
	for (int i = 0; i < 12; i++) {
		offset = i * bsize;
		if (inode.i_block[i] != 0) {
			//printf("BLOCK%d\n",i+1);
			directoryEntry(inodeNum, inode.i_block[i], offset);
		} else
			return;
	}
	for (int i = 12; i < 15; i++) {
		switch (i) {
		case 12:
			offset = 12 * bsize;
			break;
		case 13:
			offset = (12 + (bsize / sizeof(__u32 ))) * bsize;
			break;
		case 14:
			offset = (12 + (bsize / sizeof(__u32 ))
					+ (bsize / sizeof(__u32 )) * (bsize / sizeof(__u32 )))
					* bsize;
			break;
		default:
			break;
		}
		if (inode.i_block[i] != 0) {
			directoryEntry_multi(inodeNum, inode.i_block[i], i - 11, offset);
		} else
			return;
	}
}

void indirectblock(__u32 inodeNum, struct ext2_inode inode) {

	if (inode.i_block[12] != 0) {
		__u32 offset = block_offset(inode.i_block[12]);
		__u32 value[bsize];
		pread(fd, &value, bsize, offset);
		unsigned int i;
		for (i = 0; i < bsize / sizeof(__u32 ); i++) {
			if (value[i] != 0) {
				fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", inodeNum, 1,
						i + 12, inode.i_block[12], value[i]);
			}
		}
	}

	//double
	if (inode.i_block[13] != 0) {
		__u32 offset2 = block_offset(inode.i_block[13]);
		__u32 value2[bsize];
		pread(fd, &value2, bsize, offset2);
		unsigned int j;
		for (j = 0; j < bsize / sizeof(__u32 ); j++) {
			if (value2[j] != 0) {
				fprintf(stdout, "INDIRECT,%d,%d,%lu,%d,%d\n", inodeNum, 2,
						 bsize / sizeof(__u32 ) + j + 12, inode.i_block[13], value2[j]);

				__u32 offset2_1 = block_offset(value2[j]);
				__u32 value2_1[bsize];
				pread(fd, &value2_1, bsize, offset2_1);

				unsigned int k;
				for (k = 0; k < bsize / sizeof(__u32 ); k++) {
					if (value2_1[k] != 0) {
						fprintf(stdout, "INDIRECT,%d,%d,%lu,%d,%d\n", inodeNum,
								1, bsize / sizeof(__u32 ) + k + 12, value2[j], value2_1[k]);
					}
				}
			}
		}
	}

	//triple
	if (inode.i_block[14] != 0) {
		__u32 offset3 = block_offset(inode.i_block[14]);
		__u32 value3[bsize];
		pread(fd, &value3, bsize, offset3);

		unsigned int a;
		for (a = 0; a < bsize / sizeof(__u32 ); a++) {
			if (value3[a] != 0) {
				fprintf(stdout, "INDIRECT,%d,%d,%lu,%d,%d\n", inodeNum, 3,
						a + 12 + bsize / sizeof(__u32 ) + (bsize / sizeof(__u32 ))*(bsize / sizeof(__u32 )), inode.i_block[14], value3[a]);

				__u32 value3_2[bsize];
				__u32 offset3_2 = block_offset(value3[a]);
				pread(fd, &value3_2, bsize, offset3_2);

				unsigned int b;
				for (b = 0; b < bsize / sizeof(__u32 ); b++) {
					if (value3_2[b] != 0) {
						fprintf(stdout, "INDIRECT,%d,%d,%lu,%d,%d\n", inodeNum,
								2, b + 12 + bsize / sizeof(__u32 ) + (bsize / sizeof(__u32 ))*(bsize / sizeof(__u32 )), value3[a],
								value3_2[b]);

						__u32 value3_2_1[bsize];
						__u32 offset3_2_1 = block_offset(value3_2[b]);
						pread(fd, &value3_2_1, bsize, offset3_2_1);

						unsigned int c;
						for (c = 0; c < bsize / sizeof(__u32 ); c++) {
							if (value3_2_1[c] != 0) {
								fprintf(stdout, "INDIRECT,%d,%d,%lu,%d,%d\n",
										inodeNum, 1, c + 12 + bsize / sizeof(__u32 ) + (bsize / sizeof(__u32 ))*(bsize / sizeof(__u32 )),
										value3_2[b], value3_2_1[c]);
							}
						}
					}
				}
			}
		}
	}

}

void inode_summary() {
	unsigned int inode_count = superblock.s_inodes_count;
	struct ext2_inode inode;

	for (unsigned int i = 0; i < inode_count; i++) {
		int ret = pread(fd, &inode, superblock.s_inode_size,
				i * superblock.s_inode_size
						+ block_offset(group.bg_inode_table));
		if (ret < 0) {
			fprintf(stderr, "pread failed.\n");
			exit(2);
		}

		if ((inode.i_mode != 0) && (inode.i_links_count != 0)) {
			char file_type;
			if (inode.i_mode & 0x8000) {
				file_type = 'f';
				indirectblock(i + 1, inode);
			} else if (inode.i_mode & 0x4000) {
				file_type = 'd';
				directory_driver(i + 1, inode);
				indirectblock(i + 1, inode);
			} else if (inode.i_mode & 0xA000) {
				file_type = 's';
			} else
				file_type = '?';

			time_t time;

			char atime[20];
			time = inode.i_atime;
			struct tm* matime = gmtime(&time);
			strftime(atime, 20, "%D %H:%M:%S", matime);

			char ctime[20];
			time = inode.i_ctime;
			struct tm* mctime = gmtime(&time);
			strftime(ctime, 20, "%D %H:%M:%S", mctime);

			char mtime[20];
			time = inode.i_mtime;
			struct tm* mmtime = gmtime(&time);
			strftime(mtime, 20, "%D %H:%M:%S", mmtime);

			fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", i + 1,
					file_type, inode.i_mode & 0xFFF, inode.i_uid, inode.i_gid,
					inode.i_links_count, ctime, mtime, atime, inode.i_size,
					inode.i_blocks);

			if (file_type == 's' && inode.i_size < 60)
				fprintf(stdout, ",%d", inode.i_block[0]);
			else {
				int z;
				for (z = 0; z < 15; z++)
					fprintf(stdout, ",%d", inode.i_block[z]);
			}
			fprintf(stdout, "\n");

		}

	}
}

int main(int argc, char **argv) {

	if (argc != 2) {
		fprintf(stderr, "Please provide the image file name only.\n");
		exit(1);
	}

	fs_image_name = argv[1];
	if ((fd = open(fs_image_name, O_RDONLY)) < 0) {
		fprintf(stderr, "Unable to open the specified image file\n");
		exit(2);
	}

	//function calls
	superblock_sum();
	groupSummary();
	free_block_entries();
	freeinode();
	inode_summary();

	exit(0);
}

