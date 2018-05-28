/* Peer to Peer File Transfer Server Daemon  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h> 
#include <poll.h>     
#include <signal.h>  
#include <time.h>
#include <fcntl.h>
#include <netdb.h>  

#define PORT 6660  
#define SIZEB 80000


struct pollfd *fds;
struct pollfd fds_s[1];


typedef struct {
  int fd;
  int rindex; /* receiver index */
  int sindex; /* sender index */
  char name[260];
  char spin[20]; /* sender */
  char rpin[20]; /* receiver */
  _Bool nameset:1;
  _Bool connected:1;
} client; 

client *clnt;
char *buff;


void setipv4address(struct sockaddr_in * serv_addr) 
{
   serv_addr[0].sin_family = AF_INET;
   serv_addr[0].sin_addr.s_addr = INADDR_ANY;
   serv_addr[0].sin_port = htons((uint16_t)PORT); 
}


void terminate(int n) {
   free(fds);
   free(clnt);
   free(buff);
   exit(n);
}

int main(int argc, char **argv) 
{
   void daemonize();
   void setipv4address(struct sockaddr_in *); 
   void terminate(int);


   int sockfd,i,read_val,read_val_i,read_val_x,seconds,USERS;
   int tcp_protocol_number = (getprotobyname("tcp"))->p_proto;
   nfds_t nfds,x;
   int on,poll_return;
   int temp;
   int recv_buffer_size = SIZEB;
   socklen_t cli_addr_len;
   struct sockaddr_in serv_addr, cli_addr;     

   if (argc == 2) {
      USERS = atoi(argv[1]);
   }
   else if (argc == 1) {
   
      USERS = 50;
   }

   printf("recv_buffer_size = %i\n",recv_buffer_size);

     daemonize();
 

   fds_s[0].events = POLLWRNORM;
   buff = (char *) calloc(recv_buffer_size,sizeof(int));
   memset(buff,0,recv_buffer_size);
   cli_addr_len = sizeof(cli_addr);
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   seconds=55;
   on=1;
   signal(SIGINT,terminate);
   signal(SIGTERM,terminate);
   signal(SIGPIPE,SIG_IGN);
   setipv4address(&serv_addr);
   if((bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr))) == -1) 
   {
      exit(EXIT_FAILURE);
   }
   if((listen(sockfd,5)) == -1) {
      exit(EXIT_FAILURE);
   }
   nfds = 1;
   if(!(clnt = (client *) calloc (USERS+2,sizeof(client))))
   {
     terminate(EXIT_FAILURE);
   }
   if(!(fds = (struct pollfd *) calloc(USERS+2,sizeof(struct pollfd))))
   {
     terminate(EXIT_FAILURE);
   }
   fds[0].fd = sockfd;
   fds[0].events = POLLIN;
   clnt[0].fd = sockfd;
   clnt[0].name[0]='\0';
   while(1) {
      poll_return = poll(fds,nfds,2);
      for(i=0; (i < (int) nfds) && poll_return >= 0; ++i) 
      {
         if( i == 0  && (fds[i].revents & POLLIN) && poll_return > 0  ) {

            if((fds[nfds].fd = accept(sockfd,(struct sockaddr *) &cli_addr, &cli_addr_len)) != -1) 
            {
              if((int)nfds < (USERS+1)) {
                /*fcntl(fds[nfds].fd,F_SETFL,O_NONBLOCK);*/
                
          setsockopt(fds[nfds].fd,SOL_SOCKET,SO_RCVBUF,&recv_buffer_size,sizeof(recv_buffer_size));
                setsockopt(fds[nfds].fd,SOL_SOCKET,SO_KEEPALIVE,&on,sizeof(on));
                fds[nfds].events = POLLRDNORM;
                clnt[nfds].nameset = 0;
                clnt[nfds].fd = fds[nfds].fd;
                clnt[nfds].connected=1;
                clnt[nfds].rindex=-1; 
                clnt[nfds].sindex=-1; 
              }
              else {
                 close(fds[nfds].fd);
              }
            }

            else 
            {
               free(fds);
               free(clnt);
               free(buff);
               terminate(EXIT_FAILURE);
            }

            if((int)nfds < (USERS+1)) {
               ++nfds;
            }

            if(poll_return == 1) {
               break;
            }
         }

         else if( i > 0 && ( fds[i].revents & (POLLNVAL | POLLHUP | POLLERR)) ) {
            /* close this session's receiver's session */
            if(clnt[i].nameset && clnt[i].rindex != -1) {
               temp=clnt[i].rindex;
               close(clnt[clnt[i].rindex].fd);
               --nfds;
               x=nfds-clnt[i].rindex;
               if(x>0) {
                  if(clnt[i].rindex < i) {
                        memcpy( (&(clnt[clnt[i].rindex])), (&(clnt[(clnt[i].rindex)+1])),sizeof(client) * (x));
                        memcpy( (&(fds[clnt[i].rindex])), (&(fds[(clnt[i].rindex)+1])), sizeof(struct pollfd) * (x));
                        --i;
                  }
                  else {
                        memcpy( (&(clnt[clnt[i].rindex])), (&(clnt[(clnt[i].rindex)+1])),sizeof(client) * (x));
                        memcpy( (&(fds[clnt[i].rindex])), (&(fds[(clnt[i].rindex)+1])), sizeof(struct pollfd) * (x));
                  }
               }
               memset(&clnt[nfds],0,(sizeof(client)) * ((USERS+2)-nfds));
               memset(&fds[nfds],0,(sizeof(struct pollfd)) * ((USERS+2)-nfds));

               x=0;
               while(x<nfds) {
                     if(clnt[x].rindex != -1 && temp < clnt[x].rindex) {
                        --clnt[x].rindex;
                     }
                     if(clnt[x].sindex != -1 && temp < clnt[x].sindex) {
                        --clnt[x].sindex;
                     }
                     ++x;
               }



            }  

            if(!(fds[i].events & POLLNVAL)) {
               close(fds[i].fd);
            }

            --nfds;

            x=nfds-i;

            if(x>0) {
               memcpy((&(clnt[i])),(&(clnt[i+1])),sizeof(client) * (x));
               memcpy((&(fds[i])),(&(fds[i+1])),sizeof(struct pollfd) * (x));
            }

            memset(&clnt[nfds],0,(sizeof(client)) * ((USERS+2)-nfds));
            memset(&fds[nfds],0,(sizeof(struct pollfd)) * ((USERS+2)-nfds));


            --i;

            x=0;
            while(x<nfds) {
   		  if(clnt[x].rindex != -1 && clnt[x].rindex >= i) {
		     --clnt[x].rindex;
                  }
   		  if(clnt[x].sindex != -1 && clnt[x].sindex >= i) {
		     --clnt[x].sindex;
                  }
                  ++x;
            }




         }

         else if( i > 0 && (fds[i].revents & POLLRDNORM) ) {
            read_val = read(fds[i].fd,buff,recv_buffer_size);
            if(read_val <= 0) {
               buff[0] = '\0';
               /* close this session's receiver's session */
               if(clnt[i].nameset && clnt[i].rindex != -1) {
                  temp=clnt[i].rindex;
                  close(clnt[clnt[i].rindex].fd);
                  --nfds;
                  x=nfds-clnt[i].rindex;
                  if(x>0) {
                     if(clnt[i].rindex < i) {
                        memcpy( (&(clnt[clnt[i].rindex])), (&(clnt[(clnt[i].rindex)+1])),sizeof(client) * (x));
                        memcpy( (&(fds[clnt[i].rindex])), (&(fds[(clnt[i].rindex)+1])), sizeof(struct pollfd) * (x));
                        --i;
                     }
                     else {
                        memcpy( (&(clnt[clnt[i].rindex])), (&(clnt[(clnt[i].rindex)+1])),sizeof(client) * (x));
                        memcpy( (&(fds[clnt[i].rindex])), (&(fds[(clnt[i].rindex)+1])), sizeof(struct pollfd) * (x));
                     }
                  }
                  memset(&clnt[nfds],0,(sizeof(client)) * ((USERS+2)-nfds));
                  memset(&fds[nfds],0,(sizeof(struct pollfd)) * ((USERS+2)-nfds));

                  x=0;
                  while(x<nfds) {
                     if(clnt[x].rindex != -1 && temp < clnt[x].rindex) {
                        --clnt[x].rindex;
                     }
                     if(clnt[x].sindex != -1 && temp < clnt[x].sindex) {
                        --clnt[x].sindex;
                     }
                     ++x;
                  }
               }  

               if(!(read_val)) {
                  close(fds[i].fd);
               }
               --nfds;
               x=nfds-i;
               if(x>0) {
                  memcpy( (&(clnt[i])), (&(clnt[i+1])),sizeof(client) * (x));
                  memcpy( (&(fds[i])), (&(fds[i+1])), sizeof(struct pollfd) * (x));
               }

               memset(&clnt[nfds],0,(sizeof(client)) * ((USERS+2)-nfds));
               memset(&fds[nfds],0,(sizeof(struct pollfd)) * ((USERS+2)-nfds));
               --i;

               /* added */
               x=0;
               while(x<nfds) {
   		  if(clnt[x].rindex != -1 && clnt[x].rindex >= i) {
		     --clnt[x].rindex;
                  }
   		  if(clnt[x].sindex != -1 && clnt[x].sindex >= i) {
		     --clnt[x].sindex;
                  }
                  ++x;
               }

               
            }

            else {
               buff[read_val] = '\0';
/* last */
               if((clnt[i].nameset)) {
                 read_val_x=0;
                 fds_s[0].fd = clnt[clnt[i].rindex].fd;
                 while((poll_return = poll(fds_s,1,-1)) != -1 && (read_val_i = write(clnt[clnt[i].rindex].fd,&buff[read_val_x],read_val)) != read_val) {
                    read_val_x = read_val_x + read_val_i;
                    read_val  = abs(read_val_i - read_val);
                  }
                 if(clnt[clnt[i].rindex].sindex != -1) {
                    clnt[clnt[i].rindex].sindex=-1;
                    write(clnt[i].fd,"FSENT",5);
                 }
                 else {
                    write(clnt[i].fd,"OK",2);
                 }
               }
               else {
                  if(read_val < 16) {
                     /* sender */
                     if( (memcmp(buff,"SP:",3)) == 0) {
                        memcpy(clnt[i].spin,buff,read_val);
                        clnt[i].spin[(read_val)]='\0';
                        x=1;
                        while(x < nfds) {
                           /* first four PIN digits can not be the same of another PIN digit */
                           if(((int)x != i) && !(strncmp(clnt[i].spin,clnt[x].spin,7))) {
                              write(clnt[i].fd,"wrong pin",9);     
                              clnt[i].spin[0] = 0;
                              clnt[i].nameset = 0;  
                              break;
                           }
                           ++x;
                        }
                        /* sender session */
                        if(x == nfds) {
                           if((write(clnt[i].fd,clnt[i].spin,read_val)) == read_val) {
                               clnt[i].nameset = 0;  
                           }
                           else {
                               clnt[i].nameset = 0;
                           }
                        }
                     }
                     else if( (memcmp(buff,"OK",2)) == 0 && clnt[i].sindex != -1) {
                            write(clnt[clnt[i].sindex].fd,"Ready",5);
                            clnt[i].nameset=1;
                     }
                     else if((memcmp(buff,"SET",3)) == 0 && clnt[i].rindex != -1) {
                            write(clnt[i].fd,"OK",2);
                            clnt[i].nameset=1;
                     }
                     else if( (memcmp(buff,"RP:",3)) == 0) {
                        memcpy(clnt[i].rpin,buff,read_val);
                        clnt[i].rpin[(read_val)]='\0';
                        x=1;
                        while(x < nfds) {
                           /* receiver session */
                           if(clnt[x].nameset != 1 && i != ((int)x) && !(strcmp((char *)&clnt[i].rpin[1],(char *)&clnt[x].spin[1]))) {
                              if( (write(clnt[i].fd,clnt[i].rpin,read_val)) == read_val ) {
				    clnt[x].rindex=i;
                                    clnt[i].sindex=x;
                              }
                              else {
                                 clnt[i].rpin[0]=0;
                                 clnt[i].nameset=0;
                                 clnt[i].rindex=-1;
                                 clnt[i].sindex=-1;
                                 clnt[x].rindex=-1;
                                 clnt[x].sindex=-1;
                                 clnt[x].spin[0]=0;
                                 clnt[x].nameset=0;
                              }
                              break;
                           }
                           ++x;
                        }
                        /* found no PIN */
                        if(x == nfds) {
                           write(clnt[i].fd,"wrong pin",9);     
                           clnt[i].nameset=0;
                           clnt[i].rpin[0]=0;
                        } /*last*/
                     }
                  }
                  else {
                     write(clnt[i].fd,"PIN ABOVE MAX",13);
                  }
               }
            }
           
         }
         
      }
   }

   
   free(fds);
   free(clnt);
   free(buff);
   terminate(EXIT_SUCCESS);
   return 0;
}

void daemonize()
{
     pid_t pid;                           /* PROCESS ID                                                  */
     pid_t sid;                           /* SESSION ID (SAME AS PID)                                    */
     pid = fork();                        /* NEW CHILD                                                   */
     if (pid < 0)                         /* ERROR HANDLING                                              */
        exit(EXIT_FAILURE);
     if (pid > 0)
        exit(EXIT_SUCCESS);               /* SAY BYEBYE TO FATHER                                        */
     umask(0);                            /* TO BE SURE WE HAVE ACCESS TO ANY GENERATED FILE FROM DAEMON */
     sid = setsid();                      /* WE OWN OUR OWN PLACE                                        */
     if (sid < 0)                         /* ERROR HANDLING                                              */
        exit(EXIT_FAILURE);
     if((chdir("/")) < 0)                /* CHANGE TO A PATH THAT WILL ALWAYS BE THERE                  */
        exit(EXIT_FAILURE);
     close(STDIN_FILENO);                 /* CLOSE STANDARD INPUT                                        */
     close(STDOUT_FILENO);                /* CLOSE STANDARD OUTPUT                                       */
     close(STDERR_FILENO);                /* CLOSE STANDARD ERROR OUTPUT                                 */
}

