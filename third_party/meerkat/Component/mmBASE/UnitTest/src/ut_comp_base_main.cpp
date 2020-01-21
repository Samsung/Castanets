#ifdef WIN32

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif

#include <stdio.h>

extern int ut_base_comp_socket_test(int argc, char** argv);
extern int ut_base_comp_message_test(int argc, char** argv);
extern int ut_base_comp_thread_test(int argc, char** argv);
extern int ut_base_comp_task_test(int argc, char** argv);

int main(int argc, char** argv) {
  int tc = 0;
  printf("Select Test Case\n");
  printf("[1] socket test\n");
  printf("[2] message test\n");
  printf("[3] thread test\n");
  printf("[4] task test\n");
  scanf("%d", &tc);

  if (tc == 1) {
    // FILE *stream = freopen("ut_socket_input.txt", "rt", stdin);
    int count = 4;
    char* argument[4];
    char type[2] = {
        '\0',
    };
    char address[64] = {
        '\0',
    };
    char port[32] = {
        '\0',
    };

    printf("type mode - s(server) c(client)\n");
    scanf("%s", type);
    printf("type address\n");
    scanf("%s", address);
    printf("type port\n");
    scanf("%s", port);

    argument[0] = argv[0];
    argument[1] = type;
    argument[2] = address;
    argument[3] = port;
    // fclose(stream);
    ut_base_comp_socket_test(count, argument);
  } else if (tc == 2)
    ut_base_comp_message_test(argc, argv);
  else if (tc == 3)
    ut_base_comp_thread_test(argc, argv);
  else if (tc == 4)
    ut_base_comp_task_test(argc, argv);
  return 0;
  ;
}