// sludge Archiving utility
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
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
int update(int argc, char **argv, const char *mode);
int list(int argc, char **argv);
int extract(int argc, char **argv);
int removal(int argc, char **argv);


// Prototypes
int permissionPrint(mode_t perms);
int extract(int argc, char **argv);
int removal(int argc, char **argv);
int update(int argc, char **argv, const char *mode);


typedef struct header {
  off_t    file_size;
  uint32_t   hash;
  uint8_t    file_count;
} header;

typedef struct fd{
  char       file_name[256];
  mode_t     perms;
} fd;

int permissionPrint(mode_t perms) {
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


int update(int argc, char **argv, const char *mode) {
	FILE *archive = fopen(argv[1], mode);
	if (archive == NULL) {
	  fprintf(stderr,"Error opening archive %s. Exiting...\n",argv[1]);
	  return 1;
	}
	struct stat s;
	fd *file_descript;
	header file_header;
	uint32_t hash;
	bool found;
	uint8_t *buf;
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
		if (in == NULL) {
		  fprintf(stderr,"Failed to open file %s for reading",argv[i]);
		  fclose(archive);
		  return 1;
		}
		found = false;
		buf = (uint8_t*) malloc(sizeof(uint8_t) * s.st_size);
		if ((fread(buf,sizeof(uint8_t), s.st_size,in)) != s.st_size) {
		    fprintf(stderr,"Error reading data from file %s. Exiting...\n",argv[i]);
		    fclose(in);
		    fclose(archive);
		    free(buf);
		    return 1;
		}
		hash = crc32(0,buf,s.st_size);

		int loc;
		while (!feof(archive) && !found) {
		  loc = ftell(archive);
		  if (!fread(&file_header, sizeof(struct header), 1, archive)) {
		    break;
		  }
		  if (hash != file_header.hash) {
		    fseek(archive, (file_header.file_size+ file_header.file_count*(sizeof(struct fd))),SEEK_CUR);
		  } else {
		    found = true;
		    break;
		  }
		}

		if (found) {
		  file_descript = (fd*) malloc (sizeof(struct fd) * file_header.file_count);
		  fread(&file_descript,sizeof(struct fd),file_header.file_count,archive);
		  found = false;
		  for (int j = 0; j < file_header.file_count; j ++) {
		    if (strcmp(file_descript[j].file_name,argv[i]))
			found = true;
		  }
		  if (found) {
		    printf("File %s already exists in archive %s.\n",argv[i], argv[1]);
		  } else {
		    int num_items = file_header.file_count;
		    file_header.file_count++;
		    int file_start = ftell(archive);
		    FILE* temp = fopen(".temp","a+");
		    fclose(archive);
		    archive = fopen(argv[1], "r");
		    free(buf);
		    buf = (uint8_t*) malloc (sizeof(uint8_t) * 512);
		    int bytes;
		    while (loc) {
		      if((bytes = fread(&buf, sizeof(uint8_t), 512, archive)) == 512) {
			fwrite(&buf, sizeof(uint8_t), 512, temp);
			loc -= 512;
		      } else {
			fwrite(&buf, sizeof(uint8_t), bytes, temp);
			loc -= bytes;
		      }
		    }
		    fwrite(&file_header,sizeof(struct header), 1, temp);
		    fwrite(&file_descript,sizeof(struct fd), num_items, temp);
		    strcpy(file_descript[0].file_name, argv[i]);
		    file_descript[0].perms = s.st_mode;
		    fwrite(&file_descript,sizeof(struct fd), 1, temp);
		    fseek(archive, file_start, SEEK_SET);
		    while (!feof(archive)) {
		      if ((bytes = fread(&buf, sizeof(uint8_t), 512, archive)) == 0) {
			break;
		      } else {
			fwrite(&buf, sizeof(uint8_t), bytes, temp);
		      }
		    }
		    fclose(temp);
		    fclose(archive);
		    remove(argv[1]);
		    rename(".temp", argv[1]);
		    printf("File %s written to archive %s.\n",argv[i], argv[1]);
		  }
		} else {
		  fclose(archive);
		  archive = fopen(argv[1],"a+");
		  file_header.hash = hash;
		  file_header.file_size = s.st_size;
		  file_header.file_count = 1;
		  file_descript[0].perms = s.st_mode;
		  strcpy(file_descript[0].file_name, argv[i]);
		  fwrite(&file_header, sizeof(struct header), 1, archive);
		  fwrite(file_descript, sizeof(struct fd), 1, archive);
		  fwrite(buf,sizeof(uint8_t), s.st_size, archive);
		  fclose(archive);
		  printf("File %s written to archive %s.\n",argv[i], argv[1]);
		}
		fclose(in);
	        free(buf);
                free(file_descript);
	}
	return 0;
}

int list(int argc, char **argv) {
	FILE *archive = fopen(argv[1], "r");
	if (archive == NULL) {
	  fprintf(stderr,"Error opening archive %s. Exiting...\n",argv[1]);
	  return 1;
	}
	fd file_descript;
	header file_header;
	while (!feof(archive)) {
	  if (!fread(&file_header, sizeof(struct header), 1, archive)) {
	    break;
	  }
	  printf("File listing:\n");
	  for (int i = 0; i < file_header.file_count; i++) {
	    fread(&file_descript,sizeof(struct fd), 1, archive);
	    printf(" - ");
	    permissionPrint(file_descript.perms);
	    printf("\t%s\n",file_descript.file_name);
	  }
	  fseek(archive,file_header.file_size,SEEK_CUR);
	}
	fclose(archive);
	return 0;
}

int removal(int argc, char **argv) {
	if (extract(argc, argv) != 0){
		printf("Unable to extract for removal.\n");
		return 0;
	}

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
	int cur;

	while (!feof(archive)) {
		if (!fscanf(archive, "%d%s%d", heading.file_size, heading.hash, heading.file_count)) {
			break;
		}
		fscanf(archive, "%s%s", fdlinks.file_name, fdlinks.perms);
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
			cur = ftell(archive);
		} else {
			cur = 0;
		}
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
