// sludge Archiving utility
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include "crc32.h"
// TODO: Add indexing of files
// TODO: removing files from archives
// TODO: if a file is an exact copy, have archive point to the first instance of
//		 the file that is similar
// TODO: ^ use a flag to determine if it is a copy, then point to index of same
//       file.
// TODO: update list, update, extract to reflect new header format

/* New header format: 
 * size_of_file    st.size
 * hash            uint32_t
 * file_count      uint8_t
 * array of fd structs:
   - file_name       char[256]
   - perms           mode_t
*/

typedef struct header {
  off_t    file_size;
  uint32_t   hash;
  uint8_t    file_count;
} header;

typedef struct fd{
  char       file_name[256];
  mode_t     perms;
} fd;

/*int permissionPrint(mode_t perms) {
  printf( (perms & S_IRUSR) ? "r" : "-");
  printf( (perms & S_IWUSR) ? "w" : "-");
  printf( (perms & S_IXUSR) ? "x" : "-");
  printf( (perms & S_IRGRP) ? "r" : "-");
  printf( (perms & S_IWGRP) ? "w" : "-");
  printf( (perms & S_IXGRP) ? "x" : "-");
  printf( (perms & S_IROTH) ? "r" : "-");
  printf( (perms & S_IWOTH) ? "w" : "-");
  printf( (perms & S_IXOTH) ? "x" : "-");
  return 0;
}
*/

int update(int argc, char **argv, const char *mode) {
	FILE *archive = fopen(argv[1], mode);
	struct stat s;
	uint8_t buf[1024];
	for (int i = 2; i < argc; i++) {
		if (stat(argv[i], &s) == -1) {
			fprintf(stderr, "Failed to stat file %s\n", argv[i]);
			return 1;
		}
		fwrite(&s.st_size, sizeof(s.st_size), 1, archive);
		fwrite(&s.st_mode, sizeof(s.st_mode), 1, archive);
		size_t l = strlen(argv[i]);
		fwrite(&l, sizeof(size_t), 1, archive);
		fwrite(argv[i], l, 1, archive);

		FILE *in = fopen(argv[i], "r");
		while (!feof(in)) {
			size_t n = fread(buf, 1, sizeof(buf), in);
			fwrite(buf, 1, n, archive);
		}
		fclose(in);
	}
	fclose(archive);
	return 0;
}


int list(int argc, char **argv) {
	FILE *archive = fopen(argv[1], "r");
	if (archive == NULL) {
	  fprintf(stderr,"Error opening archive %s. Exiting...\n",argv[1]);
	  return 1;
	}
	struct stat s;
	uint8_t buf[1024];
	while (!feof(archive)) {
		if (!fread(&s.st_size, sizeof(s.st_size), 1, archive)) {
			break;
		}
		fread(&s.st_mode, sizeof(s.st_mode), 1, archive);
		uint32_t hash, offset;
		fread(&hash, sizeof(hash), 1, archive);
		fread(&hash, sizeof(offset), 1, archive);
		size_t l;
		fread(&l, sizeof(size_t), 1, archive);
		char name[l + 1];
		fread(name, l, 1, archive);
		name[l] = 0;
		//permissionsPrint(s.st_mode);
		printf("\t%s\n", name);
		if (!offset) {
		    fseek(archive,s.st_size,SEEK_CUR);
		}
	}
	fclose(archive);
	return 0;
}

int removal(int argc, char **argv) {
	extract(argc, argv);

	FILE *archive = fopen(argv[1], "r");
	struct stat s;
	uint8_t buf[1024];
	for(int i = 2; i < argc; i++){
		while (!feof(archive)) {
			if (!fread(&s.st_size, sizeof(s.st_size), 1, archive)) {
				break;
			}
			fread(&s.st_mode, sizeof(s.st_mode), 1, archive);
			size_t l;
			fread(&l, sizeof(size_t), 1, archive);
			char name[l + 1];
			fread(name, l, 1, archive);
			name[l] = 0;
			if(argv[1] == name) 
				break;
			while (s.st_size) {
				size_t n = fread(buf, 1, s.st_size, archive);
				s.st_size -= n;
			}
		}
		if(feof(archive)) return -1;
		int countdown = s.st_size;
		while(countdown){
			countdown -= fread(buf, 1, s.st_size, archive);
		}
		while(!feof(archive)){
			fread(buf, 1, s.st_size, archive);
			fseek(archive, -sizeof(buf), SEEK_SET);
			fwrite(&buf, sizeof(buf), 1, archive);
			fseek(archive, sizeof(buf), SEEK_SET);
		}
		fseek(archive, 0L, SEEK_END);
		truncate(argv[1], ftell(archive)-s.st_size);
		rewind(archive);
	}
	fclose(archive);
	return 0;
}

int extract(int argc, char **argv){
	FILE *archive = fopen(argv[1], "r");
	struct stat s;
	uint8_t buf[1024];
	header heading;
	fd fdlinks;

	while (!feof(archive)) {
		if (!fscanf(archive, "%d%s%d", heading.file_size, heading.hash, heading.file_count)) {
			break;
		}
		fread(archive, "%s%s", fdlinks.file_name, fdlinks.perms);
		bool extract = true;
		for (size_t i = 2; i < argc; ++i) {
			extract = false;
			if (argc == 2 || strcmp(argv[i], fdlinks.file_name) == 0) {
				extract = true;
				break;
			}
		}
		fseek(archive, sizeof(fd)*(heading.file_count-1), SEEK_SET);
		if (extract) {
		        int cur = ftell(archive);
			FILE *out = fopen(fdlinks.file_name, "w");
			int fileSizeCounter = heading.file_size;
			while (s.st_size) {
				size_t n = fread(buf, 1, heading.file_size, archive);
				fwrite(buf, n, 1, out);
				fileSizeCounter -= n;
			}
			fclose(out);
			chmod(fdlinks.file_name, fdlinks.perms);
			fseek(archive, cur, SEEK_SET);
		}
	}
	fclose(archive);
	return 0;
}

int main(int argc, char **argv) {
	const char *usage =
		"	sludge is an archiving utility\n"
		"Default: ./sludge <archiveName> <file> etc.\n"
		"	Archives all files passed to it.\n"
		"List: 	  ./sludge -l <acrhiveName>\n"
		"	Lists files from the archive.\n"
		"Append:  ./sludge -a <archiveName> <file>\n"
		"	Appends the file to the archive.\n"
		"Extract: ./sludge -e <archiveName> <file>\n"
		"	Extracts all files unless giving a filename.\n"
		"Remove: ./sludge -r <archiveName> <file>\n"
		"	Removes the specified file from the archive.\n";
	switch (getopt(argc, argv, "l:a::e::")) {
		case 'l':
			return list(argc - 1, argv + 1);
		case 'a':
			return update(argc - 1, argv + 1, "a+");
		case 'e':
			return extract(argc - 1, argv + 1);
		case 'r':
			return removal(argc - 1, argv + 1);
		default:
			if (argc < 2) {
				fprintf(stderr, "%s", usage);
				return 1;
			}
			return update(argc, argv, "w+");
	}
}
