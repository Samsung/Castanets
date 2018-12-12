#ifdef WIN32

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif

#include <stdio.h>

extern int ut_base_comp_udpserver_test(int argc, char** argv);
extern int ut_base_comp_udpclient_test(int argc, char** argv);
extern int ut_base_comp_tcpserver_test(int argc, char** argv);
extern int ut_base_comp_tcpclient_test(int argc, char** argv);

int main(int argc, char** argv) {
  int tc = 0;
  printf("Select Test Case\n");
  printf("[1] udp server test\n");
  printf("[2] udp client test\n");
  printf("[3] tcp server test\n");
  printf("[4] tcp client test\n");
  scanf("%d", &tc);

  if (tc == 1) {
	int count = 2;
	char* argument[2];
	char port[32] = { '\0', };

	printf("type port\n");
	scanf("%s", port);

	argument[0] = argv[0];
	argument[1] = port;

    ut_base_comp_udpserver_test(count, argument);
  }
  else if (tc == 2) {
	int count = 3;
	char* argument[3];
	char address[64] = { '\0', };
	char port[32] = { '\0', };

	printf("type address\n");
	scanf("%s", address);
	printf("type port\n");
	scanf("%s", port);

	argument[0] = argv[0];
	argument[1] = address;
	argument[2] = port;

	ut_base_comp_udpclient_test(count, argument);
  }
  else if (tc == 3) {
    int count = 2;
	char* argument[2];
	char port[32] = { '\0', };

	printf("type port\n");
	scanf("%s", port);

	argument[0] = argv[0];
	argument[1] = port;

	ut_base_comp_tcpserver_test(count, argument);
  }

  else if (tc == 4) {
    int count = 3;
	char* argument[3];
	char address[64] = { '\0', };
	char port[32] = { '\0', };

	printf("type address\n");
	scanf("%s", address);
	printf("type port\n");
	scanf("%s", port);

	argument[0] = argv[0];
	argument[1] = address;
	argument[2] = port;
    ut_base_comp_tcpclient_test(count, argument);
  }
  return 0;
}