#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <limits.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/wait.h>

#include "sockets.h"
#include "main.h"
#include "exec.h"
#include "cJSON.h"

int exec_command(int rep, int display, int *flags_ptr) {

   char *cmd = "/home/hacker/bin/lcd.rb";
   char buf[BUFSIZ];
   char exec_out[BUFSIZ*100];
   FILE *ptr;

   exec_out[0] = '\0';
   //if ((ptr = popen(cmd, "r")) != NULL)
   //while (fgets(buf, BUFSIZ, ptr) != NULL)
   //(void) printf("%s", buf);
   if ((ptr = popen(cmd, "r")) != NULL) {
      while(fgets(buf, BUFSIZ, ptr) != NULL) {
         //printf("%s\n", buf);
         /*char * pch = strtok (buf,";");
         while (pch != NULL) {
            printf ("Tok: %s\n",pch);
            pch = strtok (NULL, ";");
         }*/
         strcat(exec_out, buf);
      }

      (void) pclose(ptr);
   }

   printf("Final out: %s\n", exec_out);

   //char *out;
   cJSON *json;

   json=cJSON_Parse(exec_out);
   if (!json) {
      printf("Error before: [%s]\n",cJSON_GetErrorPtr());
   }
   /*else
   {

      cJSON *next = json->child;
      while(next) {
         out = cJSON_Print(next);
         printf("%s\n",out);
         free(out);
         printf("Array size: %s -> %d\n", next->string, cJSON_GetArraySize(next));
         int i;
         for (i=0;i<cJSON_GetArraySize(next);i++) {
            cJSON *subitem=cJSON_GetArrayItem(next,i);
            printf("\t%s\n", subitem->valuestring);
         }
         next = next->next;
      }

   }*/

   //int xoffs;

   if ((*flags_ptr & INITIALIZED) == 0) {
      *flags_ptr |= INITIALIZED;

      cJSON *next = json->child;
      while(next) {
         printf("Initializing: %s -> %d\n", next->string, cJSON_GetArraySize(next));

         if(cJSON_GetArraySize(next) > 0) {
            sock_printf(sock, "screen_add %s\n", next->string);
            sock_printf(sock, "screen_set %s -name (%s) -heartbeat off\n", next->string, next->string);
            sock_printf(sock, "widget_add %s one string\n", next->string);
            if(cJSON_GetArraySize(next) > 1) {
               sock_printf(sock, "widget_add %s two string\n", next->string);
            }
         }
         next = next->next;
      }
   }

   cJSON *next = json->child;
   while(next) {
      //printf("Array size: %s -> %d\n", next->string, cJSON_GetArraySize(next));
      if(cJSON_GetArraySize(next) > 0) {
         cJSON *subitem=cJSON_GetArrayItem(next,0);
         sock_printf(sock, "widget_set %s one %d %d {%s}\n", next->string, 1, 1, subitem->valuestring);
      }
      if(cJSON_GetArraySize(next) > 1) {
         cJSON *subitem=cJSON_GetArrayItem(next,1);
         sock_printf(sock, "widget_set %s two %d %d {%s}\n", next->string, 1, 2, subitem->valuestring);
      }

      /*
      int i;
      for (i=0;i<cJSON_GetArraySize(next);i++) {
         cJSON *subitem=cJSON_GetArrayItem(next,i);
         printf("\t%s\n", subitem->valuestring);
         xoffs = (lcd_wid > strlen(subitem->valuestring)) ? (((lcd_wid - strlen(subitem->valuestring)) / 2) + 1) : 1;
         sock_printf(sock, "widget_set %s one %d %d {%s}\n", next->string, xoffs, (lcd_hgt / 2), subitem->valuestring);
      }
      */
      next = next->next;
   }

   cJSON_Delete(json);

   return 0;
}

