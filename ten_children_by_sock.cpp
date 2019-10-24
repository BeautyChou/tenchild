/*------------------------------------------------------------------------------------------------
任务：
    10个子进程求和问题。用套接字进行父子进程间数据的通信。
思路：
    父进程做服务器，产生完10个子进程后就进行服务器的准备工作。子进程做客户端，连接到服务器后
就发送自己的随机数。父进程接收到随机数后进行累加。由于子进程发起连接请求时，父进程可能还没完成服务器的准备工作，
所以我们用一个信号量进行同步，子进程投入运行首先请求这个信号量，而父进程完成准备工作才释放这个信号量。
要注意的是，由于accept()和recv()都是阻塞函数，
所以服务器要用单独的线程来调用这两个函数，所以服务器将产生10个线程来分别为10个子进程服务。另外，
10个线程都要修改全局变量total，所以要进行并发控制。
考查：
    程序填空。
------------------------------------------------------------------------------------------------*/


//Windows的头文件
///*----------------------------------------------------
#include "StdAfx.h"  //预编译头
#include <stdio.h>
#include <time.h>
#include <Winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#define WINDOWS_VERSION
//----------------------------------------------------*/

//Linux的头文件
/*------------------------------------------------------
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  //inet_ntoa()函数的头文件
------------------------------------------------------*/


//全局变量
//----------------------------------------------------
#ifdef WINDOWS_VERSION
	HANDLE		g_hSem_total;				//控制累加的信号量的句柄
	HANDLE		g_hSem_child_run;			//控制子进程运行的信号量的句柄
	HANDLE		g_hThread[10];				//10个线程的句柄
#else
	union semun
	{
		int val;
		struct semid_ds *buf;
		ushort *array;
	};
	int			sem_total_id;				//控制累加的信号量的ID
	int			sem_child_run_id;			//控制子进程运行的信号量的ID
	pthread_t	thread_id[10];				//10个线程的ID
#endif
int		total;						//和
int		listen_sock;				//用于监听的套接字
short	port;
//----------------------------------------------------


int mysend(int sock, char *buf, int len, int flags) //send_num_to_parent->mysend   line 102
{
	int sent = 0, remain = len;
	while(remain > 0)
	{
		int n = send(sock, buf+sent, remain, flags);  //返回值为已发送的字节数
		if(n == -1)  //出错的最大可能是对方关闭了套接字
			break;
		remain -= n;
		sent += n;
	}
	return sent;
}

int myrecv(int sock, char *buf, int len, int flags)  //thread_function->myrecv line149
{
	int received = 0, remain = len;
	while(remain > 0)
	{
		int n = recv(sock, buf+received, remain, flags);
		if(n == 0 || n == -1)  //0是对方调用close()，-1是对方直接退出
			break;
		remain -= n;
		received += n;
	}
	return received;
}

void send_num_to_parent(int num)  //do_child->send_num_to_parent   line 205
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);  //创建套接字
	
	//下面4行进行地址结构体的赋值。注意IP地址和端口号都要转换成网络字节序
	struct sockaddr_in server_addr;
	server_addr.sin_family		= AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //将字符串表示的IP地址转换成网络字节序整数
	server_addr.sin_port		= htons(port);  //主机字节序->网络字节序
	
	connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));  //connect()是阻塞函数，直到3次握手完成为止，或是超时为止
	
	mysend(sock, (char *)&num, 4, 0);

#ifdef WINDOWS_VERSION
	closesocket(sock);
#else
	close(sock);  //子进程发送完毕 关闭连接
#endif
}

#ifndef WINDOWS_VERSION
void p(int semid)
{
	struct sembuf buf = {0, -1, 0};
	semop(semid, &buf, 1);
}

void v(int semid)
{
	struct sembuf buf = {0, 1, 0};
	semop(semid, &buf, 1);
}
#endif

#ifdef WINDOWS_VERSION
DWORD WINAPI thread_function(void *arg) 
#else
void *thread_function(void *arg) //main->thread_function  line 276
#endif
{ 
    int index = (int)arg;
	
	//准备接受连接请求
	struct sockaddr_in peer_addr;
	int size = sizeof(struct sockaddr_in);
	int comm_sock = accept(listen_sock, (struct sockaddr *)&peer_addr, &size);  //accept()是阻塞函数，直到有客户端向本地址发起连接请求。注意返回值就是对应客户端的套接字，将来的数据收发都在这个套接字上进行
	printf("parent : 线程%d接受一个连接请求\n", index);
	
	//创建下一个线程
	if(index < 9)
#ifdef WINDOWS_VERSION
		g_hThread[index + 1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_function, (void *)(index + 1), 0, NULL);
#else
		pthread_create(&thread_id[index + 1], NULL, (void *)thread_function, (void *)(index + 1)); // 1创建2 2创建3 ……
#endif
	
	//接收一个整数
	int num;
	myrecv(comm_sock, (char *)&num, 4, 0);
	printf("parent : 线程%d接收到一个整数%d\n", index, num); //myrecv 接受完成 将随机数存在num中 index则是线程

	//累加到全局变量。由于10个线程可能形成竞争，所以必须进行并发控制
#ifdef WINDOWS_VERSION
	WaitForSingleObject(g_hSem_total, INFINITE);
	total += num;
	ReleaseSemaphore(g_hSem_total, 1, NULL);
	closesocket(comm_sock);
	ExitThread(0);
	return 0;
#else
	p(sem_total_id);    //互斥信号确保每次加进来就只有一个数
	total += num;   //通过全局变量共享内存
	v(sem_total_id);
	close(comm_sock);
	pthread_exit(NULL);
#endif
}

void init_socket() //main ->init_socket  line 270
{
	//创建流式套接字，用于监听子进程发出的连接请求
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	
	//下面4行进行地址结构体的赋值。注意IP地址和端口号都要转换成网络字节序
	struct sockaddr_in server_addr;
	server_addr.sin_family		= AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //INADDR_ANY表示请操作系统自动选一个可用的本机IP
	server_addr.sin_port		= htons(port);
	
	//将套接字绑定到指定的地址
	bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));  //服务器连接第二部 绑定
	
	//将套接字置于监听状态
	listen(listen_sock, 10);  //服务器连接第三步 监听
	
	printf("parent : 服务器开始监听连接请求\n"); //第一行输出
}

void do_child(int i)  //main->do_child     line 263
{
	//p操作，将自己挂起，等服务器准备就绪再运行
#ifdef WINDOWS_VERSION
	g_hSem_child_run = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, "parent_child_sync");
	WaitForSingleObject(g_hSem_child_run, INFINITE);
	CloseHandle(g_hSem_child_run);
#else
	p(sem_child_run_id); //p的是子进程同步信号量  需要等待父进程网络建立完成
#endif
	
	srand(time(NULL) + i); //产生随机数
	int num = rand()%10;
	
	printf("child%d: %d\n", i, num);
	
	send_num_to_parent(num);
}

int main(int argc, char* argv[])
{
	//下面4行进行Windows网络环境的初始化。UNIX中不需要
#ifdef WINDOWS_VERSION
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
#endif

	if(argc == 1)  //Windows中的父进程，Linux中的父、子进程  调用main函数时只会有一个“单词”
	{
		total = 0;

		//创建并初始化信号量
#ifdef WINDOWS_VERSION
		g_hSem_total = CreateSemaphore(NULL, 1, 1, NULL);  //创建互斥信号量 保证父进程一次性只能接收到一个随机数
		g_hSem_child_run = CreateSemaphore(NULL, 0, 10, "parent_child_sync"); //创建同步信号量用于等待父进程准备好网络避免父进程网络未创建好子进程就把随机数发送出去了
#else
		sem_total_id = semget(IPC_PRIVATE, 1, 0666);
		union semun x;
		x.val = 1;
		semctl(sem_total_id, 0, SETVAL, x);//创建互斥信号量并设置初值为1
		sem_child_run_id = semget(IPC_PRIVATE, 1, 0666);
		x.val = 0;
		semctl(sem_child_run_id, 0, SETVAL, x);//创建同步信号量并设置初值为1
#endif

		//随机产生一个端口号，但要大于1024才行
		srand(time(NULL));
		port = rand()%1024 + 1024; //1024是动态端口的起始点

		//创建10个子进程，但子进程会被p操作挂起
#ifdef WINDOWS_VERSION
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		for(int i=0; i<10; i++)
		{
			//下面3行形成子进程的启动命令行，格式为:filename -端口号 -[0-9]
			char strCmdLine[256];
			strcpy(strCmdLine, argv[0]);
			sprintf(strCmdLine + strlen(strCmdLine), " -%d -%d", port, i);

			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			ZeroMemory( &pi, sizeof(pi) );
			CreateProcess(NULL, strCmdLine, NULL, NULL, false, 0, NULL, NULL, &si, &pi);
		}
#else
		int i;
		for(i=0; i<10; i++)
		{
			pid_t pid = fork();
			if(pid == 0)
			{
				do_child(i);
				return 0;
			}
		}
#endif

		//初始化网络环境，完成服务器的准备工作
		init_socket();
		
		//创建用于接受连接请求的第一个线程，将来由第一个去创建第二个，第二个创建第三个，以此类推。要保存线程的句柄或ID，因为后面主线程要等待10个子线程结束。
#ifdef WINDOWS_VERSION
		g_hThread[0] = CreateThread(NULL, 0, thread_function, (void *)0, 0, NULL);
#else
		pthread_create(&thread_id[0], NULL, (void *)thread_function, (void *)0);  //这个是第一个线程创建第二个线程
#endif

		//释放信号量，让子进程运行
#ifdef WINDOWS_VERSION
		ReleaseSemaphore(g_hSem_child_run, 10, NULL);
#else
		struct sembuf buf = {0, 10, 0};
		semop(sem_child_run_id, &buf, 1); //释放10个信号量让子进程可以开始运行
#endif

		//等待10个线程结束
		for(i=0; i<10; i++)
#ifdef WINDOWS_VERSION
			WaitForSingleObject(g_hThread[i], INFINITE);
#else
			pthread_join(thread_id[i], NULL);  //等待另外一个线程结束（从1-10）
#endif

		printf("parent : all threads ended, total=%d\n", total);
			
		//删除信号量
#ifdef WINDOWS_VERSION
		CloseHandle(g_hSem_total);
		CloseHandle(g_hSem_child_run);
#else
		semctl(sem_total_id, IPC_RMID, 0);
		semctl(sem_child_run_id, IPC_RMID, 0);
#endif
	}  //结束line 218 的if（arg==1）
	else if(argc == 3)  //Windows中的子进程，命令行格式为:filename -端口号 -[0-9] 
	{
#ifdef WINDOWS_VERSION
		sscanf(argv[1]+1, "%d", &port);
		int index;
		sscanf(argv[2]+1, "%d", &index);

		do_child(index);
#endif
	}
	else
		printf("启动命令不正确\n");
	
	return 0;
}
