/*------------------------------------------------------------------------------------------------
����
    10���ӽ���������⡣���׽��ֽ��и��ӽ��̼����ݵ�ͨ�š�
˼·��
    ����������������������10���ӽ��̺�ͽ��з�������׼���������ӽ������ͻ��ˣ����ӵ���������
�ͷ����Լ���������������̽��յ������������ۼӡ������ӽ��̷�����������ʱ�������̿��ܻ�û��ɷ�������׼��������
����������һ���ź�������ͬ�����ӽ���Ͷ������������������ź����������������׼���������ͷ�����ź�����
Ҫע����ǣ�����accept()��recv()��������������
���Է�����Ҫ�õ������߳����������������������Է�����������10���߳����ֱ�Ϊ10���ӽ��̷������⣬
10���̶߳�Ҫ�޸�ȫ�ֱ���total������Ҫ���в������ơ�
���飺
    ������ա�
------------------------------------------------------------------------------------------------*/


//Windows��ͷ�ļ�
///*----------------------------------------------------
#include "StdAfx.h"  //Ԥ����ͷ
#include <stdio.h>
#include <time.h>
#include <Winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#define WINDOWS_VERSION
//----------------------------------------------------*/

//Linux��ͷ�ļ�
/*------------------------------------------------------
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  //inet_ntoa()������ͷ�ļ�
------------------------------------------------------*/


//ȫ�ֱ���
//----------------------------------------------------
#ifdef WINDOWS_VERSION
	HANDLE		g_hSem_total;				//�����ۼӵ��ź����ľ��
	HANDLE		g_hSem_child_run;			//�����ӽ������е��ź����ľ��
	HANDLE		g_hThread[10];				//10���̵߳ľ��
#else
	union semun
	{
		int val;
		struct semid_ds *buf;
		ushort *array;
	};
	int			sem_total_id;				//�����ۼӵ��ź�����ID
	int			sem_child_run_id;			//�����ӽ������е��ź�����ID
	pthread_t	thread_id[10];				//10���̵߳�ID
#endif
int		total;						//��
int		listen_sock;				//���ڼ������׽���
short	port;
//----------------------------------------------------


int mysend(int sock, char *buf, int len, int flags) //send_num_to_parent->mysend   line 102
{
	int sent = 0, remain = len;
	while(remain > 0)
	{
		int n = send(sock, buf+sent, remain, flags);  //����ֵΪ�ѷ��͵��ֽ���
		if(n == -1)  //������������ǶԷ��ر����׽���
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
		if(n == 0 || n == -1)  //0�ǶԷ�����close()��-1�ǶԷ�ֱ���˳�
			break;
		remain -= n;
		received += n;
	}
	return received;
}

void send_num_to_parent(int num)  //do_child->send_num_to_parent   line 205
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);  //�����׽���
	
	//����4�н��е�ַ�ṹ��ĸ�ֵ��ע��IP��ַ�Ͷ˿ںŶ�Ҫת���������ֽ���
	struct sockaddr_in server_addr;
	server_addr.sin_family		= AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //���ַ�����ʾ��IP��ַת���������ֽ�������
	server_addr.sin_port		= htons(port);  //�����ֽ���->�����ֽ���
	
	connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));  //connect()������������ֱ��3���������Ϊֹ�����ǳ�ʱΪֹ
	
	mysend(sock, (char *)&num, 4, 0);

#ifdef WINDOWS_VERSION
	closesocket(sock);
#else
	close(sock);  //�ӽ��̷������ �ر�����
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
	
	//׼��������������
	struct sockaddr_in peer_addr;
	int size = sizeof(struct sockaddr_in);
	int comm_sock = accept(listen_sock, (struct sockaddr *)&peer_addr, &size);  //accept()������������ֱ���пͻ����򱾵�ַ������������ע�ⷵ��ֵ���Ƕ�Ӧ�ͻ��˵��׽��֣������������շ���������׽����Ͻ���
	printf("parent : �߳�%d����һ����������\n", index);
	
	//������һ���߳�
	if(index < 9)
#ifdef WINDOWS_VERSION
		g_hThread[index + 1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_function, (void *)(index + 1), 0, NULL);
#else
		pthread_create(&thread_id[index + 1], NULL, (void *)thread_function, (void *)(index + 1)); // 1����2 2����3 ����
#endif
	
	//����һ������
	int num;
	myrecv(comm_sock, (char *)&num, 4, 0);
	printf("parent : �߳�%d���յ�һ������%d\n", index, num); //myrecv ������� �����������num�� index�����߳�

	//�ۼӵ�ȫ�ֱ���������10���߳̿����γɾ��������Ա�����в�������
#ifdef WINDOWS_VERSION
	WaitForSingleObject(g_hSem_total, INFINITE);
	total += num;
	ReleaseSemaphore(g_hSem_total, 1, NULL);
	closesocket(comm_sock);
	ExitThread(0);
	return 0;
#else
	p(sem_total_id);    //�����ź�ȷ��ÿ�μӽ�����ֻ��һ����
	total += num;   //ͨ��ȫ�ֱ��������ڴ�
	v(sem_total_id);
	close(comm_sock);
	pthread_exit(NULL);
#endif
}

void init_socket() //main ->init_socket  line 270
{
	//������ʽ�׽��֣����ڼ����ӽ��̷�������������
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	
	//����4�н��е�ַ�ṹ��ĸ�ֵ��ע��IP��ַ�Ͷ˿ںŶ�Ҫת���������ֽ���
	struct sockaddr_in server_addr;
	server_addr.sin_family		= AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //INADDR_ANY��ʾ�����ϵͳ�Զ�ѡһ�����õı���IP
	server_addr.sin_port		= htons(port);
	
	//���׽��ְ󶨵�ָ���ĵ�ַ
	bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));  //���������ӵڶ��� ��
	
	//���׽������ڼ���״̬
	listen(listen_sock, 10);  //���������ӵ����� ����
	
	printf("parent : ��������ʼ������������\n"); //��һ�����
}

void do_child(int i)  //main->do_child     line 263
{
	//p���������Լ����𣬵ȷ�����׼������������
#ifdef WINDOWS_VERSION
	g_hSem_child_run = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, "parent_child_sync");
	WaitForSingleObject(g_hSem_child_run, INFINITE);
	CloseHandle(g_hSem_child_run);
#else
	p(sem_child_run_id); //p�����ӽ���ͬ���ź���  ��Ҫ�ȴ����������罨�����
#endif
	
	srand(time(NULL) + i); //���������
	int num = rand()%10;
	
	printf("child%d: %d\n", i, num);
	
	send_num_to_parent(num);
}

int main(int argc, char* argv[])
{
	//����4�н���Windows���绷���ĳ�ʼ����UNIX�в���Ҫ
#ifdef WINDOWS_VERSION
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
#endif

	if(argc == 1)  //Windows�еĸ����̣�Linux�еĸ����ӽ���  ����main����ʱֻ����һ�������ʡ�
	{
		total = 0;

		//��������ʼ���ź���
#ifdef WINDOWS_VERSION
		g_hSem_total = CreateSemaphore(NULL, 1, 1, NULL);  //���������ź��� ��֤������һ����ֻ�ܽ��յ�һ�������
		g_hSem_child_run = CreateSemaphore(NULL, 0, 10, "parent_child_sync"); //����ͬ���ź������ڵȴ�������׼����������⸸��������δ�������ӽ��̾Ͱ���������ͳ�ȥ��
#else
		sem_total_id = semget(IPC_PRIVATE, 1, 0666);
		union semun x;
		x.val = 1;
		semctl(sem_total_id, 0, SETVAL, x);//���������ź��������ó�ֵΪ1
		sem_child_run_id = semget(IPC_PRIVATE, 1, 0666);
		x.val = 0;
		semctl(sem_child_run_id, 0, SETVAL, x);//����ͬ���ź��������ó�ֵΪ1
#endif

		//�������һ���˿ںţ���Ҫ����1024����
		srand(time(NULL));
		port = rand()%1024 + 1024; //1024�Ƕ�̬�˿ڵ���ʼ��

		//����10���ӽ��̣����ӽ��̻ᱻp��������
#ifdef WINDOWS_VERSION
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		for(int i=0; i<10; i++)
		{
			//����3���γ��ӽ��̵����������У���ʽΪ:filename -�˿ں� -[0-9]
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

		//��ʼ�����绷������ɷ�������׼������
		init_socket();
		
		//�������ڽ�����������ĵ�һ���̣߳������ɵ�һ��ȥ�����ڶ������ڶ����������������Դ����ơ�Ҫ�����̵߳ľ����ID����Ϊ�������߳�Ҫ�ȴ�10�����߳̽�����
#ifdef WINDOWS_VERSION
		g_hThread[0] = CreateThread(NULL, 0, thread_function, (void *)0, 0, NULL);
#else
		pthread_create(&thread_id[0], NULL, (void *)thread_function, (void *)0);  //����ǵ�һ���̴߳����ڶ����߳�
#endif

		//�ͷ��ź��������ӽ�������
#ifdef WINDOWS_VERSION
		ReleaseSemaphore(g_hSem_child_run, 10, NULL);
#else
		struct sembuf buf = {0, 10, 0};
		semop(sem_child_run_id, &buf, 1); //�ͷ�10���ź������ӽ��̿��Կ�ʼ����
#endif

		//�ȴ�10���߳̽���
		for(i=0; i<10; i++)
#ifdef WINDOWS_VERSION
			WaitForSingleObject(g_hThread[i], INFINITE);
#else
			pthread_join(thread_id[i], NULL);  //�ȴ�����һ���߳̽�������1-10��
#endif

		printf("parent : all threads ended, total=%d\n", total);
			
		//ɾ���ź���
#ifdef WINDOWS_VERSION
		CloseHandle(g_hSem_total);
		CloseHandle(g_hSem_child_run);
#else
		semctl(sem_total_id, IPC_RMID, 0);
		semctl(sem_child_run_id, IPC_RMID, 0);
#endif
	}  //����line 218 ��if��arg==1��
	else if(argc == 3)  //Windows�е��ӽ��̣������и�ʽΪ:filename -�˿ں� -[0-9] 
	{
#ifdef WINDOWS_VERSION
		sscanf(argv[1]+1, "%d", &port);
		int index;
		sscanf(argv[2]+1, "%d", &index);

		do_child(index);
#endif
	}
	else
		printf("���������ȷ\n");
	
	return 0;
}
