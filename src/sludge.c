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
 * file_modes      st.mode
 * hash            uint32_t
 * dupe_flag       uint32_t   *This is for future proofing 
   offset                     (big archives with big offsets)
 * name_length     size_t
 * file_name       dependent upon value of name_length
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
		if(multi(argc, argv){
			//handle identical files
			break;
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

int multi(int argc, char **argv){
	int list(int argc, char **argv) {
	FILE *archive = fopen(argv[1], "r");
	FILE *addition = fopen(argv[2], "r");
	struct stat s;
	uint8_t buf[1024];
	uint8_t obuf[1024];
	while (!feof(archive) || !feof(addition)) {
		if (!fread(&s.st_size, sizeof(s.st_size), 1, archive) || !fread(fread(&s.st_size, sizeof(s.st_size), 1, addition)) {
			break;
		}
		fread(&s.st_mode, sizeof(s.st_mode), 1, archive);
		size_t l;
		fread(&l, sizeof(size_t), 1, archive);
		char name[l + 1];
		fread(name, l, 1, archive);
		name[l] = 0;
		//printf("%s\n", name);
		while (s.st_size) {
			size_t n = fread(buf, 1, s.st_size, archive);
			size_t z = fread(obuf, 1, s.st_size, addition);
			if(obuf != buf){
				return -1;
			}
			s.st_size -= n;
		}
	}
	fclose(archive);	
	fclose(addition);
	return 0;
}

int list(int argc, char **argv) {
	FILE *archive = fopen(argv[1], "r");
	struct stat s;
	uint8_t buf[1024];
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
		printf("%s\n", name);
		
		fseek(archive,s.st_size,SEEK_CUR);		
	}
	fclose(archive);
	return 0;
}

int remove(int argc, char **argv) {
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
			if(argv[1] = name) break;
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
		ftruncate(archive, ftell(archive)-s.st_size);
		rewind(archive);
	}
	fclose(archive);
	return 0;
}

int extract(int argc, char **argv){
	FILE *archive = fopen(argv[1], "r");
	struct stat s;
	uint8_t buf[1024];
	while (!feof(archive)) {
		if (!fread(&s.st_size, sizeof(s.st_size), 1, archive)) {
			break;
		}
		fread(&s.st_mode, sizeof(s.st_mode), 1, archive);
		size_t l;
		fread(&l, sizeof(s.st_size), 1, archive);
		char name[l + 1];
		fread(name, l, 1, archive);
		name[l] = 0;
		bool extract = true;
		for (size_t i = 2; i < argc; ++i) {
			extract = false;
			if (strcmp(argv[i], name) == 0) {
				extract = true;
				break;
			}
		}
		if (extract) {
			FILE *out = fopen(name, "w");
			while (s.st_size) {
				size_t n = fread(buf, 1, s.st_size, archive);
				fwrite(buf, n, 1, out);
				s.st_size -= n;
			}
			fclose(out);
			chmod(name, s.st_mode);
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
			return remove(argc - 1, argv + 1);
		default:
			if (argc < 2) {
				fprintf(stderr, "%s", usage);
				return 1;
			}
			return update(argc, argv, "w+");
	}
}
