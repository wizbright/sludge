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

// Prototypes
int permissionPrint(mode_t perms);
int extract(int argc, char **argv);
int removal(int argc, char **argv);
int update(int argc, char **argv, const char *mode);
int list(int argc, char **argv);

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
  FILE *archive , *temp, *in;

	struct stat s;
	fd *file_descript;
	header file_header;
	uint32_t hash;
	bool found;
	uint8_t *buf;
	int loc, cur, bytes, num_items, file_start;

	archive = fopen(argv[1], mode);
	if (archive == NULL) {
	  fprintf(stderr,"Error opening archive %s. Exiting...\n",argv[1]);
	  return 1;
	}
	for (int i = 2; i < argc; i++) {
		if (stat(argv[i], &s) == -1) {
			fprintf(stderr, "Failed to stat file %s\n", argv[i]);
			break;
		}
		in = fopen(argv[i], "r+");
		if (in == NULL) {
		  fprintf(stderr,"Failed to open file %s for reading",argv[i]);
		  fclose(archive);
		  break;
		}
		found = false;
		buf = (uint8_t*) malloc(sizeof(uint8_t) * s.st_size);
		fseek(archive, 0, SEEK_SET);
		if ((fread(buf,sizeof(uint8_t), s.st_size,in)) != s.st_size) {
		    fprintf(stderr,"Error reading data from file %s. Skipping...\n",argv[i]);
		    fclose(in);
		    fclose(archive);
		    free(buf);
		    break;
		}
		hash = crc32(0,buf,s.st_size);

		while (!feof(archive) && !found) {
		  loc = ftell(archive);
		  if (!fread(&file_header, sizeof(struct header), 1, archive)) {
		    break;
		  } else if (hash == file_header.hash) {
		    found = true;
		  } else {
		    fseek(archive, (file_header.file_size+ file_header.file_count*(sizeof(struct fd))),SEEK_CUR);
		  }
		}

		if (found) {
		  num_items = file_header.file_count;
		  file_descript = (fd*) malloc (sizeof(struct fd) * num_items);
		  fread(file_descript,sizeof(struct fd),num_items,archive);
		  found = false;
		  for (int j = 0; j < file_header.file_count; j ++) {
		    if (!strcmp(file_descript[j].file_name,argv[i])) {
			found = true;
		    }
		  }
		  if (found) {
		    printf("File %s already exists in archive %s.\n",argv[i], argv[1]);
		    break;
		  } else {
		    num_items = file_header.file_count;
		    file_header.file_count++;
		    file_start = ftell(archive);
		    temp = fopen(".temp","w+");
		    fclose(archive);
		    archive = fopen(argv[1], "r");
		    fseek(archive,0,SEEK_SET);
		    free(buf);
		    buf = (uint8_t*) malloc (sizeof(uint8_t) * 512);
		    bytes = 512;
		    while (loc != 0) {
		      if (loc < 512) {
			bytes = loc;
		      }
		      if((bytes = (fread(buf, sizeof(uint8_t), bytes, archive))) == 0) {
			
			break;
		      } else if (bytes == 512) {
			fwrite(buf, sizeof(uint8_t), 512, temp);
			loc -= 512;
		      } else {
			fwrite(buf, sizeof(uint8_t), bytes, temp);
			loc -= bytes;
		      }
		    }
		    fwrite(&file_header,sizeof(struct header), 1, temp);
		    fwrite(&file_descript,sizeof(struct fd), num_items, temp);
		    strcpy(file_descript[0].file_name, argv[i]);
		    file_descript[0].perms = s.st_mode;
		    fwrite(file_descript,sizeof(struct fd), 1, temp);
		    fseek(archive, file_start, SEEK_SET);
		    while (!feof(archive)) {
		      if ((bytes = fread(buf, sizeof(uint8_t), 512, archive)) == 0) {
			break;
		      } else {
			fwrite(buf, sizeof(uint8_t), bytes, temp);
		      }
		    }
		    fclose(temp);
		    fclose(archive);
		    remove(argv[1]);
		    rename(".temp", argv[1]);
		    printf("File %s written to archive %s.\n",argv[i], argv[1]);
		    free(buf);
		    free(file_descript);
		  }
		} else {
		  fclose(archive);
		  archive = fopen(argv[1],"a+");
		  file_descript = (fd*) malloc (sizeof(struct fd));
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
		  free(buf);
		  free(file_descript);
		}
		fclose(in);
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
	printf("File listing:\n");
	while (!feof(archive)) {
	  if (!fread(&file_header, sizeof(struct header), 1, archive)) {
	    break;
	  }

	  for (int i = 0; i < file_header.file_count; i++) {
	    fread(&file_descript,sizeof(struct fd), 1, archive);
	    printf(" - ");
	    permissionPrint(file_descript.perms);
	    printf("\t%ud",file_header.hash);
	    printf("\t%zu",file_header.file_size);
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
	struct fd *file_descript;
	struct header file_header;
	uint8_t *buf;
	int j;
	for(int i = 2; i < argc; i++){
		while (!feof(archive)) {
		  int loc = ftell(archive);
			if (!fread(&file_header, sizeof(struct header), 1, archive)) {
				break;
			}
			j = 0;
			bool found = false;
			file_descript = (fd*) malloc (sizeof(struct fd) * file_header.file_count);
			fread(file_descript, sizeof(struct fd), file_header.file_count, archive);
			while ( j < file_header.file_count) {
			  if (strcmp(argv[i], file_descript[j].file_name)) {
			    found = true;
			    break;
			  }
			  j++;
			}
			if (!found) {
			  fseek(archive, file_header.file_size, SEEK_CUR);
			} else {
			  //if is the only copy of the file
			  if (file_header.file_count == 1) {
			    int loc2 = ftell(archive);
			    fseek(archive, loc, SEEK_SET);
			    //put more stuff here
			  } else {
			    //when there is more than one file_descriptor
			  }
			}
		}
	}

	fclose(archive);
	return 0;
}

int extract(int argc, char **argv){
	FILE *archive = fopen(argv[1], "r");
	struct stat s;
	uint8_t buf[1024];
	header heading;
	fd *fdlinks;
	int cur, j, bytes;
	bool extract;

	for (int i = 2; i < argc; i++) {
	  fseek(archive, 0, SEEK_SET);
	  while (!feof(archive)) {
		
	    if (!fread(&heading, sizeof(struct header), 1, archive)) {
	      break;
	    }
	    fdlinks = (fd*) malloc (sizeof(struct fd) * heading.file_count);    
	    fread(fdlinks, sizeof(struct fd), 2, archive);
	    extract = false;
	    for (j = 0; j < heading.file_count; j++) {
	      if (!strcmp(argv[i],fdlinks[j].file_name)) {
		extract = true;
		break;
	      }
	    }
	    if (!extract) {
	      fseek(archive, heading.file_size, SEEK_CUR);
	    } else {
	      FILE *fout = fopen(argv[i],"r+");
	      if (fout == NULL) {
		fprintf(stderr, "Error opening file '%s' for writing. Skipping...\n",argv[i]);
		break;
		fclose(fout);
	      }
	      while(heading.file_size != 0) {
		if (heading.file_size < 1024) {
		  bytes = heading.file_size;
		}
		if (!(bytes = fread(buf,sizeof(uint8_t), bytes, archive))) {
		  break;
		} else {
		  fwrite(buf, sizeof(uint8_t), bytes, fout);
		  heading.file_size -= bytes;
		}
	      }
	      fclose(fout);
	      chmod(argv[i], fdlinks[j].perms);
	      free(fdlinks);
	      break;
	    }
	  }
	}
	fclose(archive);
	return 0;
}

int main(int argc, char **argv) {
	const char *usage =
		"	sludge is an archiving utility\n"
		"Create: ./sludge -c <archiveName> <file> etc.\n"
		"	Archives all files passed to it.\n"
		"List: 	  ./sludge -l <acrhiveName>\n"
		"	Lists files from the archive.\n"
		"Append:  ./sludge -a <archiveName> <file>\n"
		"	Appends the file to the archive.\n"
		"Extract: ./sludge -e <archiveName> <file>\n"
		"	Extracts all files unless giving a filename.\n"
		"Remove: ./sludge -r <archiveName> <file>\n"
		"	Removes the specified file from the archive.\n"
		"Default: Shows this~\n";
	switch (getopt(argc, argv, "l:a::e::c::")) {
       	case 'l':
			return list(argc - 1, argv + 1);
		case 'a':
			return update(argc - 1, argv + 1, "r+");
		case 'e':
			return extract(argc - 1, argv + 1);
		case 'r':
			return removal(argc - 1, argv + 1);
		case 'c':
			return update(argc -1, argv + 1, "w+");
		default:
			fprintf(stderr, "%s", usage);
			return 1;
	}
}
