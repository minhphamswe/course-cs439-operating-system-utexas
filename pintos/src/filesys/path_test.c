#include "filesys/path.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char** argv)
{
  printf("Test 1.1: path_dirname('/usr/bin/sort')\n");
  printf("\tExpect: '/usr/bin' \t Got: '%s'\n", path_dirname("/usr/bin/sort"));
  printf("Test 1.1: path_dirname('usr/bin/sort')\n");
  printf("\tExpect: 'usr/bin' \t Got: '%s'\n", path_dirname("usr/bin/sort"));
  printf("Test 1.3: path_dirname('stdio.h')\n");
  printf("\tExpect: '.' \t Got: '%s'\n", path_dirname("stdio.h"));

  printf("Test 2.1: path_basename('/usr/bin/sort')\n");
  printf("\tExpect: 'sort' \t Got: '%s'\n", path_basename("/usr/bin/sort"));
  printf("Test 2.2: path_basename('stdio.h')\n");
  printf("\tExpect: 'stdio.h' \t Got: '%s'\n", path_basename("stdio.h"));

  printf("Test 3.1: path_isabs('/usr/bin/sort')\n");
  printf("\tExpect: 1 \t Got: %d\n", path_isabs("/usr/bin/sort"));
  printf("Test 3.2: path_isabs('bin/sort')\n");
  printf("\tExpect: 0 \t Got: %d\n", path_isabs("bin/sort"));
  printf("Test 3.2: path_isabs('stdio.h')\n");
  printf("\tExpect: 0 \t Got: %d\n", path_isabs("stdio.h"));

  printf("Test 4.1: path_join2('/usr/', '/usr/bin/sort')\n");
  printf("\tExpect: '/usr/bin/sort' \t Got: '%s'\n",
         path_join2("/usr/", "/usr/bin/sort"));
  printf("Test 4.2: path_join2('usr/', '/usr/bin/sort')\n");
  printf("\tExpect: '/usr/bin/sort' \t Got: '%s'\n",
         path_join2("usr/", "/usr/bin/sort"));
  printf("Test 4.3: path_join2('usr', '/usr/bin/sort')\n");
  printf("\tExpect: '/usr/bin/sort' \t Got: '%s'\n",
         path_join2("usr", "/usr/bin/sort"));

  printf("Test 4.4: path_join2('/usr/', 'bin/sort')\n");
  printf("\tExpect: '/usr/bin/sort' \t Got: '%s'\n",
         path_join2("/usr/", "bin/sort"));
  printf("Test 4.5: path_join2('usr/', 'bin/sort')\n");
  printf("\tExpect: 'usr/bin/sort' \t Got: '%s'\n",
         path_join2("usr/", "bin/sort"));
  printf("Test 4.6: path_join2('usr', 'bin/sort')\n");
  printf("\tExpect: 'usr/bin/sort' \t Got: '%s'\n",
         path_join2("usr", "bin/sort"));
}