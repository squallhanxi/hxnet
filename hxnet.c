#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>

#define REMOTE_PORT 8787
#define HEADER "hxroute"
#define ROUTE_LEN 48
#define BROADCAST_BUFF 1024
#define BROADCAST_TIME 10

pthread_t pt_broadcast_table,pt_recive_table;
char *interfaceIP;
int bcIP[4] = {0};
struct bc_table
{
	char protocol_header[7];
	int table_count;
	int route_table[];
};
struct bc_table *rs_table;
struct bc_table *last_table;

int check_route(int *rt1, int *rt2)
{
	int ret=0;
	int i=0;
	for(i=0; i<ROUTE_LEN; i++)
	{
		if((rt1+i)!=(rt2+i))
		{
			ret=1;
		}
	}
	return ret;
}

void *format_ipv4_toint(char rt[], int IP_int[4])//格式化IPV4地址为int类型
{
	IP_int[0] = 0;
	IP_int[1] = 0;
	IP_int[2] = 0;
	IP_int[3] = 0;
	char *IPAddr_str[4];
	int i = 0;
	char *p;
	char *buff = (char *)malloc(16);
	strcpy(buff, rt);
	//buff = rt;
	p = strsep(&buff, ".");
	while(p)
	{
		IPAddr_str[i++] = p;
		p = strsep(&buff, ".");
	}
	if(IPAddr_str[0]!=NULL && IPAddr_str[1]!=NULL && IPAddr_str[2]!=NULL && IPAddr_str[3]!=NULL)
	{
		IP_int[0] = atoi(IPAddr_str[0]);
		IP_int[1] = atoi(IPAddr_str[1]);
		IP_int[2] = atoi(IPAddr_str[2]);
		IP_int[3] = atoi(IPAddr_str[3]);
	}
}

char *format_ipv4_tostr(int IP_int[4])//格式化IPV4地址为字符串类型
{
	static char tmp[16] = {0}; 
	char str1[4], str2[4], str3[4], str4[4];
	sprintf(str1, "%d", IP_int[0]);
	sprintf(str2, "%d", IP_int[1]);
	sprintf(str3, "%d", IP_int[2]);
	sprintf(str4, "%d", IP_int[3]);
	strcpy (tmp, str1);
	strcat (tmp, ".");
	strcat (tmp, str2);
	strcat (tmp, ".");
	strcat (tmp, str3);
	strcat (tmp, ".");
	strcat (tmp, str4);
	return tmp;
}

int GetTxtLine(const char *filename)//获取路由表行数
{
	FILE *fd;
	int count = 0;
	if (fd = fopen(filename,"r"))
	{
		while (!feof(fd))
		{
			if ('\n' == fgetc(fd))
			{
				count ++;
			}
		}
	}
	if (fd)
	{
		fclose(fd);
	}
	return count;
}


char *get_ipv4(char *fileaddr, int rtline, int type)
{
	FILE *fp;
	char ch;
	char str1[512];
	int line = 0;
	int pos = 0;
	int i=0;
	static char *ret;
	ret = (char *)malloc(sizeof(char) * 16);
	if((fp=fopen(fileaddr,"rt")) == NULL)  
	{  
		printf("cannot open file! \n");
		exit(1);
	}
	ch=fgetc(fp);  
	while(ch!=EOF)  
	{
		if(ch!='\n')
		{
			str1[pos++] = ch;
			ch=fgetc(fp);
		}
		else
		{
			if(rtline == line)
			{
				str1[pos] = '\0';
				pos = 0;
				//printf("%s",str1);
				line++;
				ch=fgetc(fp);

				switch(type)
				{
					case 0://获取目的地址
						while(str1[i]!=' ')
						{
							*(ret+i) = str1[i];
							i++;
						}
						break;
					case 1://获取网关
						while(str1[16+i]!=' ')
						{
							*(ret+i) = str1[16+i];
							i++;
						}
						break;
					case 2://获取掩码
						while(str1[32+i]!=' ')
						{
							*(ret+i) = str1[32+i];
							i++;
						}
						break;
					default:
						printf("type error!\r\n");
				}
			}
			else
			{
				str1[pos] = '\0';
				pos = 0;
				line++;
				ch=fgetc(fp);
			}
		}
	}  
	fclose(fp);
	return ret;
}

char *set_buff(char interIP[16])
{
	struct bc_table *send_table;
	send_table = (struct bc_table *)malloc(1000);
	int destIP[4] = {0};
	int gateway[4] = {0};
	int genmask[4] = {0};
	int rtline = 0;
	int i;
	int j=0;

	system("route >/root/routetable");
	rtline = GetTxtLine("/root/routetable");
	printf("route line:%d\n",rtline);

	send_table->table_count = 0;

	char new_gw[16] = {0};
	strcpy(new_gw, interIP);

	for(i=2; i<rtline; i++)
	{
		char *d_IP = get_ipv4("/root/routetable",i,0);
		char *gw = get_ipv4("/root/routetable",i,1);
		char *mask = get_ipv4("/root/routetable",i,2);

		format_ipv4_toint(d_IP, destIP);
		format_ipv4_toint(gw, gateway);
		format_ipv4_toint(mask, genmask);

		format_ipv4_toint(new_gw, gateway);//用本机地址替换网关

		strcpy(send_table->protocol_header, HEADER);
		if(destIP[0] != 0 && genmask[0] != 0)//判断是否为有效路由
		{
			send_table->table_count++;
			send_table->route_table[j*12+0] = destIP[0];
			send_table->route_table[j*12+1] = destIP[1];
			send_table->route_table[j*12+2] = destIP[2];
			send_table->route_table[j*12+3] = destIP[3];
			send_table->route_table[j*12+4] = gateway[0];
			send_table->route_table[j*12+5] = gateway[1];
			send_table->route_table[j*12+6] = gateway[2];
			send_table->route_table[j*12+7] = gateway[3];
			send_table->route_table[j*12+8] = genmask[0];
			send_table->route_table[j*12+9] = genmask[1];
			send_table->route_table[j*12+10] = genmask[2];
			send_table->route_table[j*12+11] = genmask[3];
			j++;
		}
	}
	
	return (char *)send_table;
}

int update_table(struct bc_table *rs_table, struct bc_table *last_table)
{
	int i=0, j=0;
	for(i=0;i<rs_table->table_count;i++)
	{
		for(j=0;j<last_table->table_count;j++)
		{
			if(check_route((rs_table->route_table)+i*ROUTE_LEN, (last_table->route_table)+j*ROUTE_LEN)!=0)
			{
				int d[4] = {0};
				int g[4] = {0};
				int m[4] = {0};
				char cmd[128] = {0};
				d[0] = rs_table->route_table[0+12*i];
				d[1] = rs_table->route_table[1+12*i];
				d[2] = rs_table->route_table[2+12*i];
				d[3] = rs_table->route_table[3+12*i];
				g[0] = rs_table->route_table[4+12*i];
				g[1] = rs_table->route_table[5+12*i];
				g[2] = rs_table->route_table[6+12*i];
				g[3] = rs_table->route_table[7+12*i];
				m[0] = rs_table->route_table[8+12*i];
				m[1] = rs_table->route_table[9+12*i];
				m[2] = rs_table->route_table[10+12*i];
				m[3] = rs_table->route_table[11+12*i];
				strcpy(cmd,"route add -net ");
				strcat(cmd,format_ipv4_tostr(d));
				strcat(cmd," netmask ");
				strcat(cmd,format_ipv4_tostr(m));
				strcat(cmd," gw ");
				strcat(cmd,format_ipv4_tostr(g));
				printf("add route:%s\n",cmd);
				system(cmd);
			}
		}
	}

	for(i=0;i<last_table->table_count;i++)
	{
		for(j=0;j<rs_table->table_count;j++)
		{
			if(check_route((rs_table->route_table)+j*ROUTE_LEN, (last_table->route_table)+i*ROUTE_LEN)!=0)
			{
				int d[4] = {0};
				int m[4] = {0};
				char cmd[128] = {0};
				d[0] = last_table->route_table[0+12*i];
				d[1] = last_table->route_table[1+12*i];
				d[2] = last_table->route_table[2+12*i];
				d[3] = last_table->route_table[3+12*i];
				m[0] = last_table->route_table[8+12*i];
				m[1] = last_table->route_table[9+12*i];
				m[2] = last_table->route_table[10+12*i];
				m[3] = last_table->route_table[11+12*i];
				strcpy(cmd,"route del -net ");
				strcat(cmd,format_ipv4_tostr(d));
				strcat(cmd," netmask ");
				strcat(cmd,format_ipv4_tostr(m));
				printf("del route:%s\n",cmd);
				//system(cmd);
			}
		}
	}

	return 0;
}

static void* pthread_broadcast_table(void *arg)
{
	int iFd;
	int iOptval = 1;
	struct sockaddr_in Addr;

	if ((iFd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("socket fail\n");
		return NULL;
	}
    
	if (setsockopt(iFd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &iOptval, sizeof(int)) < 0)
	{
		printf("setsockopt failed!");
	}
	memset(&Addr, 0, sizeof(struct sockaddr_in));
	Addr.sin_family = AF_INET;
	Addr.sin_addr.s_addr = inet_addr(format_ipv4_tostr(bcIP));
	Addr.sin_port = htons(REMOTE_PORT);

	char *send_buf;
	char interIP[16] = {0};
	strcpy(interIP, interfaceIP);
	int k;
	while (1)
	{
		send_buf = set_buff(interIP);
		printf("active route:%d\n",((struct bc_table *)send_buf)->table_count);
		int send_buf_len = ((struct bc_table *)send_buf)->table_count * 48 + 11;
		printf("broadcast:%d-->%s\n", REMOTE_PORT,format_ipv4_tostr(bcIP));
		for(k=0;k<(send_buf_len - 11)/4;k++)
		{
			printf("%d ", ((struct bc_table *)send_buf)->route_table[k]);
		}
		printf("\n");
		if ((sendto(iFd, send_buf, send_buf_len, 0, (struct sockaddr*)&Addr, sizeof(struct sockaddr))) == -1)
		{
			printf("sendto fail, errno=%d\n", errno);
			return NULL;
		}
		sleep(BROADCAST_TIME);
	}
	close(iFd);

	return 0;
}

static void* pthread_recive_table(void *arg)
{
	int iAddrLength;
	char rgMessage[BROADCAST_BUFF];
	int iOptval = 1;
	int iFd;
	int myIP[4] = {0};
	char interIP[16] = {0};
	struct sockaddr_in Addr;

	if ((iFd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("socket fail\n");
		return NULL;
	}

	if (setsockopt(iFd, SOL_SOCKET, SO_REUSEADDR, &iOptval, sizeof(int)) < 0)
	{
		printf("setsockopt failed!\n");
	}
	memset(&Addr, 0, sizeof(struct sockaddr_in));
	Addr.sin_family = AF_INET;
	Addr.sin_addr.s_addr = INADDR_ANY;
	Addr.sin_port = htons(REMOTE_PORT);
	iAddrLength = sizeof(Addr);

	if (bind(iFd, (struct sockaddr *)&Addr, sizeof(Addr)) == -1)
	{
		printf("bind failed!\n");
	}

	strcpy(interIP, interfaceIP);

	while (1)
	{
		if (recvfrom(iFd, rgMessage, sizeof(rgMessage), 0, (struct sockaddr *)&Addr, &iAddrLength) == -1)
		{
			printf("recv failed!\n");
		}
		else
		{
			format_ipv4_toint(interIP, myIP);
			if(((unsigned char *)&Addr.sin_addr)[0] != myIP[0] || ((unsigned char *)&Addr.sin_addr)[1] != myIP[1] || \
				((unsigned char *)&Addr.sin_addr)[2] != myIP[2] || ((unsigned char *)&Addr.sin_addr)[3] != myIP[3])	
			{
				rs_table = (struct bc_table *)rgMessage;
				if(strcmp(rs_table->protocol_header, HEADER) == 0)
				{
					printf("recv msg\n");
					update_table(rs_table, last_table);
					last_table = rs_table;
				}
			}
		}
	}

	close(iFd);

	return 0;
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("arg error!\r\n");
		return 1;
	}
	interfaceIP = (char *)malloc(16);
	interfaceIP = argv[1];

	format_ipv4_toint(interfaceIP, bcIP);
	bcIP[3] = 255;

	last_table = (struct bc_table *)malloc(sizeof(struct bc_table));
	last_table->table_count = 1;
	last_table->route_table[0] = 0;
	last_table->route_table[1] = 0;
	last_table->route_table[2] = 0;
	last_table->route_table[3] = 0;
	last_table->route_table[4] = 0;
	last_table->route_table[5] = 0;
	last_table->route_table[6] = 0;
	last_table->route_table[7] = 0;
	last_table->route_table[8] = 0;
	last_table->route_table[9] = 0;
	last_table->route_table[10] = 0;
	last_table->route_table[11] = 0;

	printf("route start!\r\nIP address is:%s\r\n", interfaceIP);
	if ((pthread_create(&pt_broadcast_table, NULL, pthread_broadcast_table, NULL)) == -1)
	{
		printf("create pt_broadcast_table error!\n");
		return 1;
	}

	if ((pthread_create(&pt_recive_table, NULL, pthread_recive_table, NULL)) == -1)
	{
		printf("create pt_recive_table error!\n");
		return 1;
	}
	
	while(1);

	return 0;
}


