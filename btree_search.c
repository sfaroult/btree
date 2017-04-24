/* ---------------------------------------------------------------- *
 *
 *             btree_search.c
 *
 *  Search in a B-tree
 *
 * ---------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "debug.h"

static void find_key_loc(NODE_T *n, char *key, KEYLOC_T *locptr, short lvl) {
    short     i = 1;
    int       cmp = -1;

    // Find the leaf node where the key should be stored
    if (n && key && locptr) {
      debug(lvl, "searching node %hd", n->id);
      while ((i <= n->keycnt)
             && ((cmp = btree_keycmp(key, n->k[i].key)) > 0)) {
        i++;
      }
      if (cmp == 0) {
        // We've found it in the tree
        debug(lvl, "** found at position %hd", i);
        locptr->n = n;
        locptr->pos = i;
        return;
      } else {
        if (cmp > 0) { // Key searched is bigger than
                       // last key in the node
          find_key_loc(n->k[n->keycnt].bigger, key, locptr, lvl+2);
        } else {
          find_key_loc(n->k[i-1].bigger, key, locptr, lvl+2);
        }
      }
    }
}

extern KEYLOC_T btree_find_key(char *key) {
    KEYLOC_T  loc = {NULL, 0};
    debug(0, "looking for key location");
    find_key_loc(btree_root(), key, &loc, 0);
    return loc;
}

// The following function is merely to display the search path
static char search_tree(char *key, NODE_T *t, short lvl) {
   // Returns 1 if found, 0 if not
   char ret = 0;
   int  i;
   int  cmp = -1;

   if (key && t) {
     for (i = 0; i < lvl; i++) {
       putchar(' ');
     }
     i = 1;
     while ((i <= t->keycnt)
            && ((cmp = btree_keycmp(key, t->k[i].key)) > 0)) {
       if (i) {
         putchar(',');
       }
       if (btree_numeric()) {
         printf("%d", (int)(t->k[i].key));
       } else {
         printf("%s", t->k[i].key);
       }
       i++;
     }
     if (cmp == 0) {
       printf(" *** found\n");
       ret = 1;
     } else {
       putchar('\n');
       return search_tree(key, t->k[i-1].bigger, lvl+2);
     }
   } else {
     printf("*** NOT FOUND ***\n");
   }
   return ret;
}

extern void   btree_search(char *key) {
  int     numkey;
  NODE_T *root = btree_root();

  if (key) {
    printf("Search path:\n");
    if (btree_numeric()) {
      numkey = atoi(key);
      (void)search_tree((char *)&numkey, root, 0);
    } else {
      (void)search_tree(key, root, 0);
    }
  }
}
