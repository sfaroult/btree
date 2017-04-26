#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

#include "btree.h"
#include "bt.h"
#include "debug.h"

#define LINE_LEN          2048
#define KEY_MAXLEN         250
#define OPTIONS      "xeunqp:dk:" 

#define SHOW_NOTHING         0
#define SHOW_TREE            1
#define SHOW_LIST            2

static char G_extended = 0;
static char G_id = 1;
static char G_echo = 0;
static char G_prompt = 1;

static char *read_key(FILE *input) {
    char  buffer[KEY_MAXLEN +1];
    char *p;
    int   len;

    if (fgets(buffer, KEY_MAXLEN + 1, input) != NULL) {
      p = buffer;
      while (isspace(*p)) {
        p++;
      }
      len = strlen(p);
      while (len && isspace(p[len-1])) {
        len--;
      }
      if (len) {
        p[len] = '\0';
        return strdup(p); 
      }
    }
    return NULL;
}

static void  list(NODE_T *n) {
    short i;

    if (n) {
      for (i = 0; i <= n->keycnt; i++) {
        if (n->k[i].key) {
          if (btree_numeric()) {
            printf(" %d", *((int*)(n->k[i].key)));
          } else {
            printf(" %s", n->k[i].key);
          }
        }
        list(n->k[i].bigger);
      }
    }
}

extern void  btree_show_node(NODE_T *n) {
   short i;

   assert(n);
   if (G_id) {
     printf("%3d-", n->id);
   }
   fflush(stdout);
   putchar('[');
   if (!G_extended) {
     for (i = 1; i <= n->keycnt; i++) {
       if (i > 1) {
         putchar(' ');
       }
   fflush(stdout);
       if (btree_numeric()) {
         printf("%d", *((int*)(n->k[i].key)));
       } else {
         printf("%s", n->k[i].key);
       }
   fflush(stdout);
       if (i < n->keycnt) {
         putchar(',');
       }
   fflush(stdout);
     }
   } else {
     for (i = 0; i <= btree_maxkeys(); i++) {
       if (n->k[i].key) {
         if (btree_numeric()) {
           printf("%d", *((int*)(n->k[i].key)));
         } else {
           printf("%s", n->k[i].key);
         }
   fflush(stdout);
       } else {
         if (i) {
           putchar('*');
         }
   fflush(stdout);
       }
       if (n->k[i].bigger) {
         if (G_id) {
           printf("<%hd>", (n->k[i].bigger)->id);
         } else {
           putchar(':');
         }
   fflush(stdout);
       } else {
         putchar('~');
   fflush(stdout);
       }
     }
     if (n->parent) {
       printf("(^%hd)", (n->parent)->id);
       fflush(stdout);
     }
   }
   printf("]\n");
   fflush(stdout);
}

extern void  btree_display(NODE_T *n, int blanks) {
  if (n) {
    int  i;

    for (i = 1; i <= blanks; i++) {
      putchar(' ');
    }
    btree_show_node(n);
    for (i = 0; i <= n->keycnt; i++) {
      btree_display(n->k[i].bigger, blanks + 3);
    }
  }                             /* End of if */
  fflush(stdout);
}                               /* End of btree_display() */


static void usage(char *prog) {
   fprintf(stdout, "Usage: %s [flags]\n", prog);
   fprintf(stdout, "  Flags:\n");
   fprintf(stdout, "    -p <filename>: preload <filename>\n");
   fprintf(stdout,
           "    -k <n>       : store at most <n> keys per node (default %d)\n",
           btree_maxkeys());
   fprintf(stdout,
       "    -x           : extended display - show links and empty slots\n");
   fprintf(stdout,
       "    -q           : quiet; don't display tree after changes\n");
   fprintf(stdout, "    -e           : echo value added/removed\n");
   fprintf(stdout, "    -u           : unique (no duplicate keys)\n");
   fprintf(stdout, "    -n           : numeric values\n");
}

int main(int argc, char **argv) {
  char  *key;
  int    ch;
  FILE  *fp;
  int    preloaded = 0;
  char   feedback = SHOW_TREE;
  int    maxkeys;
  char   read_cmd = 1;
  char   line[LINE_LEN];
  char  *p;
  char  *q;
  int    len;
  int    kw;

  while ((ch = getopt(argc, argv, OPTIONS)) != -1) {
    switch (ch) {
      case 'p':   // Preload
        if ((fp = fopen(optarg, "r")) != NULL) {
          while ((key = read_key(fp)) != NULL) {
            btree_insert(key);
            preloaded++;
          }
          fclose(fp);
        } else {
          perror(optarg);
        } 
        break;
      case 'x':
        G_extended = 1;
        break;
      case 'e':
        G_echo = 1;
        break;
      case 'd':
        debug_on();
        break;
      case 'q':
        feedback = SHOW_NOTHING;
        G_prompt = 0;
        break;
      case 'n':
        if (preloaded) {
           fprintf(stderr, "Option -n must precede option -p <filename>\n");
           btree_free();
           exit(1);
        }
        btree_setnumeric();
        break;
      case 'u':
        if (preloaded) {
           fprintf(stderr, "Option -u must precede option -p <filename>\n");
           btree_free();
           exit(1);
        }
        btree_setunique();
        break;
      case 'k':
        if (preloaded) {
           fprintf(stderr, "Option -k <n> must precede option -p <filename>\n");
           btree_free();
           exit(1);
        }
        if (!sscanf(optarg, "%d", &maxkeys)) {
          printf("Invalid max number of keys - using %d\n", btree_maxkeys());
        }
        btree_setmaxkeys(maxkeys);
        break;
      case '?':
      default:
        usage(argv[0]);
        exit(1);
    }
  }
  debug_off(); // In case it was turned-on for preload
  argc -= optind;
  argv += optind;
  if (preloaded) {
    printf("Preloaded data:\n");
    btree_display(btree_root(), 0);
  }
  printf("Enter \"help\" for available commands.\n");
  while (read_cmd) {
    if (G_prompt) {
      if (debugging()) {
        printf("DBG B-TREE> ");
      } else {
        printf("B-TREE> ");
      }
    }
    if (fgets(line, LINE_LEN, stdin) == NULL) {
      btree_free();
      printf("Goodbye\n");
      break;
    }
    p = line;
    while(isspace(*p)) {
      p++;
    }
    len = strlen(p);
    while (len && isspace(p[len-1])) {
      len--;
    }
    if (len) {
      p[len] = '\0';
      q = p;
      while(*q && !isspace(*q)) {
        q++;
      }
      if (isspace(*q)) {
        *q++ = '\0';
        while(isspace(*q)) {
          q++;
        }
      }
      switch((kw = bt_search(p))) {
          case BT_ID:
              G_id = 1;
              break;
          case BT_NOID:
              G_id = 0;
              break;
          case BT_AUTOTREE:
              feedback = SHOW_TREE;
              break;
          case BT_AUTOLIST:
              feedback = SHOW_LIST;
              break;
          case BT_HUSH:
              feedback = SHOW_NOTHING;
              break;
          case BT_TRC :
              debug_on();
              break;
          case BT_NOTRC :
              debug_off();
              break;
          case BT_ADD :
          case BT_INS :
              if (G_echo) {
                printf("+%s\n", q);
                fflush(stdout);
              }
              btree_insert(q);
              if (feedback) {
                if (feedback == SHOW_TREE) {
                   btree_display(btree_root(), 0);
                } else {
                   list(btree_root());
                }
                putchar('\n');
              }
              break;
          case BT_DEL :
          case BT_REM :
              if (G_echo) {
                printf("-%s\n", q);
                fflush(stdout);
              }
              btree_delete(q);
              if (feedback) {
                if (feedback == SHOW_TREE) {
                   btree_display(btree_root(), 0);
                } else {
                   list(btree_root());
                }
                putchar('\n');
              }
              break;
          case BT_FIND :
          case BT_SEARCH :
              btree_search(q);
              break;
          case BT_LIST :
              list(btree_root());
              putchar('\n');
              break;
          case BT_SHOW :
          case BT_DISPLAY :
              btree_display(btree_root(), 0);
              putchar('\n');
              break;
          case BT_HELP :
              printf("Available commands:\n");
              printf(" help                       : display this\n");
              printf(" ins <key> or add <key>     : insert a key\n");
              printf(" rem <key> or del <key>     : remove a key\n");
              printf(" find <key> or search <key> : display search path\n");
              printf(" id                         : display id next to node (default)\n");
              printf(" noid                       : suppress id next to node\n");
              printf(" show or display            : display the tree\n");
              printf(" list                       : list ordered keys\n");
              printf(" hush                       : display nothing after change\n");
              printf(" autotree                   : show tree after change (default)\n");
              printf(" autolist                   : show ordered list after change\n");
              printf(" trc                        : display extensive trace\n");
              printf(" notrc                      : turn tracing off\n");
              printf(" bye, quit or stop          : quit the program\n");
              break;
          case BT_BYE :
          case BT_QUIT :
          case BT_STOP :
              read_cmd = 0;
              btree_free();
              printf("Goodbye\n");
              break;
          default:
              printf("Invalid command \"%s\"\n", p);
              break;
      }     /* End of switch */
    }       /* End of if */
  }         /* End of while */
  return 0;
}           /* End of main() */
