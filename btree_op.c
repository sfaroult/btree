/* ----------------------------------------------------------------- *
 *
 *                         btree_op.c
 *
 *  Everything but insert and delete.
 *
 * ----------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

#include "btree.h"
#include "debug.h"

static short   G_maxkeys = DEF_MAX_KEYS;
static float   G_fillrate = DEF_FILL_RATE;
static NODE_T *G_root = NULL;
static char    G_unique = 0;
static char    G_numeric = 0;

extern void btree_setmaxkeys(short n) {
  G_maxkeys = n;
}

extern short btree_maxkeys(void) {
  return G_maxkeys;
}

extern float btree_fillrate(void) {
  return G_fillrate;
}

extern void btree_setroot(NODE_T *n) {
  G_root = n;
  if (n) {
    n->parent = NULL;
  }
}

extern NODE_T *btree_root(void) {
  return G_root;
}

extern void btree_setunique(void) {
  G_unique = 1;
}

extern char btree_unique(void) {
  return G_unique;
}

extern void btree_setnumeric(void) {
  G_numeric = 1;
}

extern char btree_numeric(void) {
  return G_numeric;
}

extern int btree_keycmp(char *k1, char *k2) {
  // Returns 0 if k1 == k2,
  //        a value > 0 if k1 > k2
  //        a value < 0 if k1 < k2
  int cmp;

  if (G_numeric) {
    if (*((int *)k1) == *((int *)k2)) {
      cmp = 0;
    } else if (*((int *)k1) > *((int *)k2)) {
      cmp = 1;
    } else {
      cmp = -1;
    }
  } else {
    cmp = strcmp(k1, k2);
  }
  return cmp;
}

extern char btree_check(NODE_T *n, char *prev_key) {
  // Debugging - check that everything is OK in the tree
  static char *last_key;

  if (n) {
    int  i;

    assert((n->parent == NULL)
           || ((n->keycnt <= btree_maxkeys()) && (n->keycnt >= MIN_KEYS)));
    if (prev_key == NULL) {
      last_key = prev_key;
    }   
    for (i = 0; i <= btree_maxkeys(); i++) {
      if (n->k[i].key) {
        if (i == 0) {
          // Should be null
          debug(0, "Slot %d in node %hd: key should be null", i, n->id);
          return 1;
        }
        if (last_key) {
          if (btree_keycmp(n->k[i].key, last_key) < 0) {
            debug(0, "Slot %d in node %hd: misplaced key", i, n->id);
            return 1;  // Inconsistent, current key
                       // should be greater than the previous one
          }
        }
        if (i > n->keycnt) {
          // Should be null
          debug(0, "Slot %d in node %hd: key should be null", i, n->id);
          return 1;
        }
        last_key = n->k[i].key;
      } else {
        if (i && (i <= n->keycnt)) {
          debug(0, "Slot %d in node %hd: no value, keycnt is %hd",
                i, n->id, n->keycnt);
          return 1;
        }
      }
      if (n->k[i].bigger) {
        if (i <= n->keycnt) {  
          if ((n->k[i].bigger)->parent != n) {
            debug(0, "Parent pointer of node %hd inconsistent",
                     (n->k[i].bigger)->id);
            return 1;
          }
          if (btree_check(n->k[i].bigger, n->k[i].key)) {
            return 1;
          }
        } else {
          debug(0, "Slot %d in node %hd: bigger pointer should be null",
                   i, n->id);
          return 1;
        }
      }
    }
  }                             /* End of if */
  return 0;
}                               /* End of btree_check() */

extern char *key_duplicate(char *key) {
    char *dupl = NULL;
    if (key) {
      if (G_numeric) {
        // A union would be more space efficient, but
        // would complicate the code unnecessarily for
        // what shouldn't be the most common case.
        int val = *((int *)key);

        if ((dupl = (char *)malloc(sizeof(int))) != NULL) {
          memcpy(dupl, &val, sizeof(int));
        }
      } else { 
        dupl = strdup(key);
      }
    }
    return dupl;
}

extern NODE_T *new_node(NODE_T *parent) {
    static short last_id = 0;

    NODE_T *n =(NODE_T *)malloc(sizeof(NODE_T));
    assert(n);
    last_id++;
    n->parent = parent;
    n->id = last_id;
    n->keycnt = 0;
    n->k = (REDIRECT_T *)calloc((1 + G_maxkeys), sizeof(REDIRECT_T));
    assert(n->k);
    return n;
}

static void free_tree(NODE_T **root_ptr) {
    if (root_ptr && *root_ptr) {
      int i;

      for (i = 0; i < (*root_ptr)->keycnt; i++) {
        free_tree(&((*root_ptr)->k[i].bigger));
        if ((*root_ptr)->k[i].key) {
          free((*root_ptr)->k[i].key);
        }
      }
      free((*root_ptr)->k);
      free(*root_ptr);
      *root_ptr = NULL;
    }
}

extern void btree_free(void) {
    free_tree(&G_root);
}

extern NODE_T *left_sibling(NODE_T *n, short *sep_pos) {
   // Find the node on the left
   // sep_pos is the index of the key in the parent
   // that is greater than all keys in the left sibling
   // and smaller than all keys in the current node.
   NODE_T *l = NULL;
   NODE_T *p;

   if (n && sep_pos && ((p = n->parent) != NULL)) {
     if (n == p->k[0].bigger) {
       *sep_pos = -1;
       return NULL;
     }
     assert(p->k);
     // Where are we?
     *sep_pos = 1;
     while ((*sep_pos <= p->keycnt)
            && (p->k[*sep_pos].bigger != n)) {
       (*sep_pos)++;
     }
     assert(*sep_pos <= p->keycnt);
     l = p->k[(*sep_pos)-1].bigger;
   } 
   return l;
}

extern NODE_T *right_sibling(NODE_T *n, short *sep_pos) {
   // Find the node on the right
   // See comments in left_sibling
   NODE_T *r = NULL;
   NODE_T *p;

   if (n && sep_pos && ((p = n->parent) != NULL)) {
     assert(p->k);
     *sep_pos = 0; 
     // Where are we?
     while ((*sep_pos <= p->keycnt)
            && (p->k[*sep_pos].bigger != n)) {
       (*sep_pos)++;
     }
     assert(p->k[*sep_pos].bigger == n);
     if (*sep_pos == p->keycnt) {
       *sep_pos = -1;
       r = NULL;
     } else {
       (*sep_pos)++;
       r = p->k[*sep_pos].bigger;
     } 
  }
  return r;
}

extern NODE_T *merge_leaf_nodes(NODE_T *left, char *sep_key,
                                NODE_T *right, short lvl) {
   // Left and right are assumed to be two sibling nodes,
   // sep_key is the key in the parent that is bigger than all keys
   // in the left node and smaller than all keys in the right node.
   // It can be null if we are suppressing the last key in the node
   // and want to merge the two subtrees.
   // Merges sep_key + "right" into "left" and returns "left"
   if (left && right
       && _is_leaf(left)
       && _is_leaf(right)
       && (left->parent == right->parent) 
       && ((left->keycnt
           + right->keycnt
           + (sep_key ? 1 : 0)) <= G_maxkeys)) {
     debug(lvl, "merging node %hd and node %hd", left->id, right->id);
     if (sep_key) {
       left->k[left->keycnt+1].key = strdup(sep_key);
       (left->keycnt)++;
     }
     (void)memmove(&(left->k[left->keycnt+1]),
                   &(right->k[1]),
                   sizeof(REDIRECT_T) * right->keycnt);
     left->keycnt += right->keycnt; 
     // Nothing else to free than the containers as
     // keys are moved and kept
     free(right->k);
     free(right);
     if (debugging()) {
       int i;
       char node_buffer[1024];
       char key_buffer[20];

       strcpy(node_buffer, "[");
       for (i = 1; i <= left->keycnt; i++) {
         if (i > 1) {
           strncat(node_buffer, ",", 1023 - strlen(node_buffer));
         }
         if (btree_numeric()) {
           sprintf(key_buffer, "%d", *((int*)(left->k[i].key)));
           strncat(node_buffer, key_buffer, 1023 - strlen(node_buffer));
         } else {
           strncat(node_buffer, left->k[i].key, 1023 - strlen(node_buffer));
         } 
       } 
       strncat(node_buffer, "]", 1023 - strlen(node_buffer));
       debug(lvl, "merged (node %hd): %s", left->id, node_buffer);
     }
   }
   return left;
}

extern NODE_T *find_node(NODE_T *tree, char *key) {
    // Find the leaf node where the key should be stored
    if (tree && key) {
       if (!_is_leaf(tree)) {
         short i = 1;
         int   cmp = -1;

         while ((i <= tree->keycnt)
                && ((cmp = btree_keycmp(key, tree->k[i].key)) > 0)) {
           i++;
         }
         if (cmp == 0) {
           // We've found it in the tree
           if (G_unique) {
             return NULL;
           }
           return tree;
         } else {
           return find_node(tree->k[i-1].bigger, key);
         }
       } else {
         // Leaf - can only insert there
         return tree;
       }
    }
    return NULL;
}

extern short find_pos(NODE_T *n, char *key, char present, short lvl) {
    // 'present' says whether the key is expected to be
    // present or absent
    short pos = -1;
    int   cmp = 1;

    if (n && key) {
      // Find where the key should go.
      // Note that we don't worry whether the node
      // is full or not.
      pos = 1;
      while ((pos <= n->keycnt)
             && ((cmp = btree_keycmp(key, n->k[pos].key)) > 0)) {
        pos++;
      }
      if (pos > 1) {
        if (G_numeric) {
          debug(lvl, "%d at pos %hd of node %hd (after %d)",
                *((int*)key), pos, n->id,
                *((int *)(n->k[pos-1].key)));
        } else {
          debug(lvl, "%s at pos %hd of node %hd (after %s)",
                  key, pos, n->id, n->k[pos-1].key);
        }
      } else {
        debug(lvl, "goes at pos 1");
      }
      if (present) {
        // Expected to be found
        if (cmp) {   // Not here
          debug(lvl, "key was expected to be found and not here");
          return -1;
        }
      } else {
        // Not expected to be found
        // Not a problem if not unique, problem if unique
        if (G_unique && (cmp == 0)) {
          debug(lvl, "key found and was NOT expected");
          return -1;
        }
      }
    }
    return pos;
}
