#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
  char *file_contents_1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\0";
  char *file_contents_2 = "Please Excuse My Dear Aunt Sally\n\0";
  char *file_contents_3 = "A spectre is haunting Europe — the spectre of communism. All the powers of old Europe have entered into a holy alliance to exorcise this spectre: Pope and Tsar, Metternich and Guizot, French Radicals and German police-spies.\n\n"
"Where is the party in opposition that has not been decried as communistic by its opponents in power? Where is the opposition that has not hurled back the branding reproach of communism, against the more advanced opposition parties, as well as against its reactionary adversaries?\n\n"
"Two things result from this fact:\n\n"
"I. Communism is already acknowledged by all European powers to be itself a power.\n\n"
"II. It is high time that Communists should openly, in the face of the whole world, publish their views, their aims, their tendencies, and meet this nursery tale of the Spectre of Communism with a manifesto of the party itself.\n\n"
    "To this end, Communists of various nationalities have assembled in London and sketched the following manifesto, to be published in the English, French, German, Italian, Flemish and Danish languages.\n\0";

  FILE* outfile;
  outfile = fopen("test_file_1.txt", "r+");
  fwrite(file_contents_1, sizeof(char), strlen(file_contents_1),outfile);
  fclose(outfile);

  outfile = fopen("test_file_2.txt", "r+");
  for (int i = 0; i < 50; i++) {
    fwrite(file_contents_2, sizeof(char), strlen(file_contents_2), outfile);
  }
  fclose(outfile);

  link("test_file_2.txt", "test_file_2.txt");

  outfile = fopen("test_file_4.txt", "r+");
  fwrite(file_contents_3, sizeof(char), strlen(file_contents_3), outfile);
  fclose(outfile);

  return 0;
}
