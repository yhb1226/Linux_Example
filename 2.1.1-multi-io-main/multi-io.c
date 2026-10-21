#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>

// block
void *client_thread(void *arg)
{

	int clientfd = *(int *)arg;

	while (1)
	{

		char buffer[128] = {0};
		int count = recv(clientfd, buffer, 128, 0);
		if (count == 0)
		{
			break;
		}

		//

		send(clientfd, buffer, count, 0);
		printf("clientfd: %d, count: %d, buffer: %s\n", clientfd, count, buffer);
	}

	close(clientfd);
}

// tcp
int main()
{

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(2048);

	if (-1 == bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr)))
	{
		perror("bind");
		return -1;
	}

	listen(sockfd, 10);

#if 0
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
	printf("accept\n");

#if 0

	char buffer[128] = {0};
	int count = recv(clientfd, buffer, 128, 0);
	send(clientfd, buffer, count, 0);
	printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfd, clientfd, count, buffer);

#else

	while (1) {

		char buffer[128] = {0};
		int count = recv(clientfd, buffer, 128, 0);
		if (count == 0) {
			break;
		}
		send(clientfd, buffer, count, 0);
		printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfd, clientfd, count, buffer);

	}
#endif

#elif 0 // 建立线程

	while (1)
	{

		struct sockaddr_in clientaddr;
		socklen_t len = sizeof(clientaddr);
		int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);

		pthread_t thid;
		pthread_create(&thid, NULL, client_thread, &clientfd);
	}

/*
	select缺点：
	1.参数多
	2.每一次都需要把rset复制到内核，内核检测完再带出来
	3.对IO的数量有限制
*/
#elif 0 // select 检测多少IO可读可写

	// int nready = select(maxfd, rset, wset, eset, timeout);//maxfd用于判断内部循环的最大值，fd依次增加---0、1、2→stdin out error--3、4、5、6-→3是listen，4 5 6即为建立的三个连接，r和w是用来判断 fd 对应的 I/O 是否就绪，e表示需要特别处理的 I/O 事件（一般不使用），只要有事件select都会返回，没有事件也会在timeout也会返回

	fd_set rfds, rset;
	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);

	int maxfd = sockfd;

	printf("loop\n");

	while (1)
	{

		rset = rfds;

		int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(sockfd, &rset))
		{
			struct sockaddr_in clientaddr;
			socklen_t len = sizeof(clientaddr);

			int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);

			printf("sockfd: %d\n", clientfd);

			FD_SET(clientfd, &rfds);
			maxfd = clientfd;
		}

		// close

		// rfds --> i , FD_CLR

		int i = 0;
		for (i = sockfd + 1; i <= maxfd; i++)
		{

			if (FD_ISSET(i, &rset))
			{ //

				char buffer[128] = {0};
				int count = recv(i, buffer, 128, 0);
				if (count == 0)
				{
					printf("disconnect\n");
					// close(i);

					FD_CLR(i, &rfds); // 都关闭才可以
					close(i);
					continue;
				}

				send(i, buffer, count, 0);
				printf("clientfd: %d, count: %d, buffer: %s\n", i, count, buffer);
			}
		}
	}
#elif 0 // poll,手动实现

	struct pollfd fds[1024] = {0};

	/*
	fd      → 看谁
	events  → 关注什么
	revents → 实际发生了什么
	*/
	fds[sockfd].fd = sockfd;
	fds[sockfd].events = POLLIN;

	int maxfd = sockfd;

	while (1)
	{

		int nready = poll(fds, maxfd + 1, -1);

		if (fds[sockfd].revents & POLLIN)
		{

			struct sockaddr_in clientaddr;
			socklen_t len = sizeof(clientaddr);

			int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);

			printf("sockfd: %d\n", clientfd);
			fds[clientfd].fd = clientfd;
			fds[clientfd].events = POLLIN;

			maxfd = clientfd;
		}

		int i = 0;
		for (i = sockfd + 1; i <= maxfd; i++)
		{

			if (fds[i].revents & POLLIN)
			{

				char buffer[128] = {0};
				int count = recv(i, buffer, 128, 0);
				if (count == 0)
				{
					printf("disconnect\n");

					fds[i].fd = -1;
					fds[i].events = 0;

					close(i);

					continue;
				}

				send(i, buffer, count, 0);
				printf("clientfd: %d, count: %d, buffer: %s\n", i, count, buffer);
			}
		}
	}
/*
question：
	1.epoll有无mmap
	2.epoll是否线程安全

	fs/eventpoll.c

	水平触发：LT
	当设计的buffer无法一次性接收到全部数据时候，如果采用的是水平触发模式，那么就是一组一组发送，直到发完

	边沿触发：ET
	来一次则会触发一次，一次性发送数据不完整并且回显的是不完整的数据

	用途：
	tcp黏包->处理方法：
	1.length (-)[----------]前面括号是length后面是content内容
	short length = 0;
	recv(fd, &length, 2, 0);
	length = ntohs(length);
	recv(fd, buffer, length, 0);

	2.分隔符
*/
#else	// epoll⭐️⭐️⭐️⭐️⭐️⭐️⭐️

	int epfd = epoll_create(1); // int size

	// pthread_create();

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;

	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

	struct epoll_event events[1024] = {0};
	while (1)
	{

		int nready = epoll_wait(epfd, events, 1024, -1);

		int i = 0;
		for (i = 0; i < nready; i++)
		{

			int connfd = events[i].data.fd;
			if (sockfd == connfd) // listen fd面向io实现
			{

				struct sockaddr_in clientaddr;
				socklen_t len = sizeof(clientaddr);

				int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);

				ev.events = EPOLLIN | EPOLLET; // 与上EPOLLET->边沿触发
				ev.data.fd = clientfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);

				printf("clientfd: %d\n", clientfd);
			}
			else if (events[i].events & EPOLLIN) // client fd
			{

				char buffer[10] = {0};
				int count = recv(connfd, buffer, 10, 0);
				if (count == 0)
				{
					printf("disconnect\n");

					epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL);
					close(i);

					continue;
				}

				send(connfd, buffer, count, 0);
				printf("clientfd: %d, count: %d, buffer: %s\n", connfd, count, buffer);
			}
		}
	}

#endif

	getchar();
	// close(clientfd);
}
