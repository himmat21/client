# client
This is for my case study assignment.

/* peer to peer client */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT 6660
#define SEND_BUFFER_SIZE 120000

int open_file(char *path,char op) {
   int fd;
   if(op == 's') {
      op = 'r';
   }
   else if(op == 'r') {
      op = 'w';
   }
   if(op == 'w') {
      if((fd = open(path,O_WRONLY | O_CREAT | O_APPEND | O_TRUNC,S_IRUSR | S_IWUSR)) == -1) {
         perror("in open_file() write");
      } 
   }
   else if(op == 'r') {
      if((fd = open(path,O_RDONLY)) == -1) {
         perror("in open_file() read");
      } 
   }
   else {
      fd = -1;
      fprintf(stderr,"in open_file(): wrong operation\n");
   }
   return fd;
}


int read_file(int filep, int *buff, int len) {
  int ret;
  
  ret = read(filep,buff,len);
  return ret;
}


int write_file(int filep, int *buff, int len) {
  int ret;
  
  ret = write(filep,buff,len);
  return ret;
}


int ipv4_tcp_connect(char *host) {
   struct sockaddr_in destination;
   int fd;
   struct hostent * serverhost;
   serverhost = gethostbyname(host);
   fd = socket(AF_INET,SOCK_STREAM,0);
   memset((void *) &destination,0,sizeof(destination));
   memcpy((void *) &destination.sin_addr.s_addr, (void *) serverhost->h_addr, (size_t) serverhost->h_length); 
   destination.sin_port = htons(PORT);
   destination.sin_family = AF_INET;
   if((connect(fd,(struct sockaddr *) &destination,sizeof(destination))) == -1) {
      perror("in connect()");
      exit(EXIT_FAILURE);
   }
   return fd;
}

struct pollfd fds_s[1];

int main(int argc, char **argv) {
   int fd; /* file descriptor */
   int nfd; /* inet file descriptor */
   int read_val; /* bytes read */
   int fread_val;
   int poll_return;
   int write_return;
   size_t filelen;
   char *host; /* network host */
   char op; /* read or write operation */
   char *file; /* filename to write or read from  */
   char rfile[200]; 
   char pin[10];
   char buff[257];
   int *bin;
   int send_buffer_size = SEND_BUFFER_SIZE;
   _Bool pinaccepted = 0;
   fds_s[0].events = POLLRDNORM;
   
    if(argc == 4) {
      fprintf(stdout,"PIN?(max 10): ");
      fgets(pin,11,stdin); /* read pin number */
      if( (feof(stdin) || ferror(stdin)) ) { /* error handling */
         clearerr(stdin);
         fprintf(stderr,"error while reading from console\n");
         exit(EXIT_FAILURE);
      }
      if(pin[(strlen(pin))-1] == '\n') {
         pin[(strlen(pin))-1] = 0; 
      }
      host = argv[1]; /* node */
      op = argv[2][0]; /* <s> or <r> */
      buff[0]=0; /* strcat(see below) makes a concat on null byte */
      file = argv[3]; /* filepath */

      if(op == 'r') {
         rfile[0]='\0';
         strncat(rfile,file,(sizeof(rfile))-4);
         if(file[(strlen(file))-1] != '/') {
            strcat(rfile,"/");
         }
      }
      if(op == 's') { /* sender operation */
         strcat(buff,"SP:");
         strcat(buff,pin);
         pin[0]=0;
         strcat(pin,buff);
         nfd = ipv4_tcp_connect(host);
         fds_s[0].fd = nfd;

         while((setsockopt(nfd,SOL_SOCKET,SO_SNDBUF,&send_buffer_size,sizeof(send_buffer_size))) == -1 ) {
            
            if(errno == ENOBUFS) {
               send_buffer_size -= 100;
            }
            else {
               perror("on setsockopt():");
	       close(nfd);
	       exit(EXIT_FAILURE);
            }
         }

         if(!(bin = (int *) calloc(send_buffer_size,sizeof(int)))) {
            perror("on calloc()");
            exit(EXIT_FAILURE);
         }

         write(nfd,buff,(strlen(buff)));
         read_val=1;
         while(read_val > 0) {

             read_val = read(nfd,buff,255);

             if(read_val > 0 ) {
                buff[read_val]=0;
                if(!(strcmp(buff,pin))) {
                   printf("PIN Accepted: %s\nWaiting for receiver\n",buff);
                }
                else if(!(strcmp(buff,"Ready"))) {
                   filelen = strlen(file);
                   --filelen;
                   while(file[filelen] == '/') {
                      file[filelen] = 0;
                      --filelen;
                   }
                   while(filelen > 0) {
                      if(file[filelen] == '/') {
                         ++filelen;
                         break;
                      }
                      --filelen;
                   }
                   printf("Ready to send\n");
                   write(nfd,"SET",3);
                   read(nfd,buff,2);
                   if(buff[0] == 'O' && buff[1] == 'K') {
		      if((size_t)(write(nfd,(char *)&file[filelen],
 			             (strlen((char *)&file[filelen])))) != (strlen((char *)&file[filelen]))) 
                      {
        		printf("on write()\n");
                      }
                      else {
                         /* test */
                         sleep(3);
                         /* ^ test ^ */
                         read(nfd,buff,5);
                         if(buff[0] == 'F' && buff[1] == 'S' && buff[2] == 'E' && buff[3] == 'N' && buff[4] == 'T') {
                            /* last */
                            if( (fd = open_file(file,op)) == -1) {
                                free(bin);
			        close(nfd);
                                exit(EXIT_FAILURE);
                            }
                            else {
                               while((fread_val = read_file(fd,bin,send_buffer_size)) != -1 && fread_val != 0) {
                                  if((write_return = write(nfd,bin,fread_val)) == -1) {
                                     perror("on write()");
                                     close(fd);
                                     free(bin);
				     exit(EXIT_FAILURE);
                                  }
                                  read(nfd,buff,2);
                                  buff[2]=0;
                                  printf("fread_val = %i, write_return = %i, buff = %s\n",fread_val,write_return,buff);
                                  /* ^ ^ */
                               } 
                               if(fread_val == -1) {
                                  close(nfd);
                               }
                               else {
                                  fds_s[0].fd=nfd;
                                  while((poll(fds_s,1,20000)) != 0) {
                                     read(nfd,buff,2);
                                  }
                                  fprintf(stdout,"End of File %s\n",buff);
                                  close(fd);
                                  close(nfd);
                               }
                               break;
                            }
                         }
                      }
                   }
                }
                else {
                   printf("PIN Error: %s\n",buff);
                   close(nfd);
                   break;
                }
             }

             else {
                if(read_val == 0) {
                   fprintf(stderr,"Connection Closed\n");
                   close(nfd);
                }
                else {
                   perror("in read()");
                }
             }

         }
         free(bin);
      }
      else if(op == 'r') {
         strcat(buff,"RP:");
         strcat(buff,pin);
         pin[0]=0;
         strcat(pin,buff);
         nfd = ipv4_tcp_connect(host);
         if(!(bin = (int *) calloc(send_buffer_size,sizeof(int)))) {
            perror("on calloc()");
            close(nfd);
            exit(EXIT_FAILURE);
         }
         write(nfd,buff,(strlen(buff)));
         read_val=1;
         while(read_val > 0) {
             read_val = read(nfd,buff,255);
             if(read_val > 0 ) {
                buff[read_val]=0;
                if(!(strcmp(buff,pin))) {
                   printf("PIN Accepted: %s\nWaiting for sender\n",buff);
		   pinaccepted=1;
                   write(nfd,"OK",2);
                }
		else if(pinaccepted) {
		   /* last start storing */
                   strcat(rfile,buff);
                   printf("Writing to: %s\n",rfile);
                   if( (fd = open_file(rfile,op)) == -1) {
                      close(nfd);
                      free(bin);
                      exit(EXIT_FAILURE);
                   } 
                   while((read_val = read(nfd,bin,80000)) != -1 && read_val != 0) {
                      if((fread_val = write_file(fd,bin,read_val)) == -1) {
                         close(nfd);
                         free(bin);
                         exit(EXIT_FAILURE);
                      }
                      printf("read_val = %i\n",read_val);
                   }
                   if(read_val == 0) {
                      printf("Connection Closed\n");
		      close(fd);
                      close(nfd);
                   }
                   else {
                      perror("on read()");
                      close(fd);
                   }
                   break;
                }
                else {
                   printf("PIN Error: %s\n",buff);
                   close(nfd);
                   break;
                }
             }

             else {
                if(read_val == 0) {
                   fprintf(stderr,"Connection Closed\n");
                   close(nfd);
                }
                else if(read_val == -1) {
                   perror("in read()");
                }
             }

         }
         free(bin);
      }
      
   }

   else {
      fprintf(stderr,"Desc:\nSend or Receive a file using a PIN Number\n\nUsage:\nSend a file through host\n\t%s <host> s <file>\n\nReceive a file to be stored in file directory through host\n\t%s <host> r <file directory>\n",argv[0],argv[0]);
   }

   return 0;
}
