/* ----------------------------------------------------------------- *
 *
 *                         btree_ins.c
 *
 *  Insertion into a btree
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


static short split_position(short target_pos, char *new_up) {
    // Reminder: the split position is the 
    // position of the key that moves up.
    // Everything smaller remains in the left node,
    // everything bigger or equal goes to a newly
    // created right sibling node.
    short maxkeys = btree_maxkeys();

    assert(new_up);
    *new_up = 0;
    if (maxkeys % 2) {
      // Odd maximum number of keys.
      // Split position is in the middle
      return (maxkeys + 1) / 2;
    } else {
      // Even maximum number of keys.
      // We aim to have the same number of keys
      // in the left node and in the right node
      // after the split - it all depends on
      // where the key to insert should go
      // Suppose we have 2p nodes. Take default
      // as p+1
      short pos = (maxkeys / 2) + 1;
      // So, left = p keys, key p+1 goes up, p - 1
      // keys to migrate to the right. But but but ...
      // This is all well if the target position is
      // greater than p+1, because it would be 
      // nicely balanced.
      // If the key to insert is smaller (or equal to)
      // the key at split_pos, then it will go to the left
      // node and we should have p-1 keys on the left
      // and p keys on the right.
      if (target_pos == pos) {
        *new_up = 1;
      }
      if (target_pos <= pos) {
        pos--;
      }
      return pos;
    }
    return -1; 
}

static NODE_T  *split_node(NODE_T *n, short split_pos, short indent) {
   // Splits n at position split_pos (moves everything
   // from split_pos + 1 to the end into a new node to the
   // right) and returns a pointer to the new sibling node.
   // indent is just for indenting debugging messages.
   NODE_T *new_n = NULL;
   short   i;

   assert(n
          && (n->keycnt == btree_maxkeys())
          && (split_pos > 0)
          && (split_pos < n->keycnt));
   debug(indent, "splitting node %hd", n->id);
   if (n->parent == NULL) {
     debug(indent, "splitting the root");
     // We are splitting the root. We need a new root
     n->parent = new_node(NULL);
     debug(indent, "new root node %hd", (n->parent)->id);
     btree_setroot(n->parent);
   }
   new_n = new_node(n->parent); // New sibling, same parent
   assert(new_n);
   debug(indent, "created new node %hd, parent %hd",
         new_n->id, (new_n->parent)->id);
   // Copy nodes 
   debug(indent, "splitting at %hd", split_pos);
   if (debugging()) {
     debug_no_nl(indent, "before split: ");
     btree_show_node(n);
   }
   // (dest, src, size)
   (void)memmove(&(new_n->k[1]), &(n->k[split_pos+1]),
                    sizeof(REDIRECT_T) * (n->keycnt - split_pos));
   // Blank out what was moved for safety
   (void)memset(&(n->k[split_pos+1]), 0,
                    sizeof(REDIRECT_T) * (n->keycnt - split_pos));
   // Adjust key counts
   new_n->keycnt = n->keycnt;
   n->keycnt = split_pos;
   if (debugging()) {
     debug_no_nl(indent, "-> %hd keys in node %d ", n->keycnt, n->id);
     btree_show_node(n);
   }
   new_n->keycnt -= split_pos;
   if (debugging()) {
     debug_no_nl(indent, "-> %hd keys in node %d ", new_n->keycnt, new_n->id);
     btree_show_node(new_n);
   }
   if (!_is_leaf(n)) {
     // Change parent pointer in the new node
     debug(indent, "updating parent pointer in children of new node");
     // Note that at this point the 'smaller' node isn't known
     for (i = 1; i <= new_n->keycnt; i++) {
       (new_n->k[i].bigger)->parent = new_n;
     }
   }
   return new_n;
}

static short insert_in_node(NODE_T  *n,
                            char    *key,
                            NODE_T  *smaller,
                            NODE_T  *bigger,
                            short    indent) {
  // Physically insert into a node
  assert(key);
  if (n == NULL) {
    debug(indent, "need to create a new root");
    NODE_T *root = new_node(NULL);
    root->keycnt = 1;
    root->k[0].bigger = smaller; 
    root->k[1].key = key;
    root->k[1].bigger = bigger; 
    btree_setroot(root);
    if (debugging()) {
      debug_no_nl(indent, "new root is node %hd ", root->id);
      btree_show_node(root);
    }
  } else {
    short pos = find_pos(n, key, 0, indent);
    if (debugging()) {
      if (btree_numeric()) {
        debug_no_nl(indent, "inserting key %d at pos %hd in node %hd ",
                    *((int*)key), pos, n->id);
      } else {
        debug_no_nl(indent, "inserting key %s at pos %hd in node %hd ",
                    key, pos, n->id);
      }
      btree_show_node(n);
    }
    if (pos >= 0) {
      if (n->keycnt == btree_maxkeys()) {
        // Must split
        char   *key_up;
        char    new_up = 0; // Flag
        short   split_pos = split_position(pos, &new_up);
        if (new_up) {
          // The key that will go up is the one being inserted
          key_up = key;
        } else {
          key_up = n->k[split_pos].key;
        }
        NODE_T *new_n = split_node(n, split_pos, indent);
        if (!new_up) {
          // A key that already was in the node (at split_pos)
          // moves up.
          // The left-most smaller pointer of the new node
          // is what was bigger than the key that moves
          // and smaller than the key that followed, now the
          // first one in the new node
          debug(indent, "moving up the key already at %hd",
                        split_pos);
          new_n->k[0].bigger = n->k[split_pos].bigger;
          if (new_n->k[0].bigger) {
            (new_n->k[0].bigger)->parent = new_n;
          }
          // Clear what refers to the promoted value (key already saved)
          n->k[split_pos].key = NULL;
          n->k[split_pos].bigger = NULL;
          (n->keycnt)--;
          // Move up the key at split_pos in the left sibling (n)
          if (insert_in_node(n->parent, key_up,
                             n, new_n, indent+2) >= 0) {
            if (pos <= split_pos) {
              // Insert into n
              return insert_in_node(n, key, smaller, bigger, indent+2);
            } else {
              // Insert into new_sibling
              return insert_in_node(new_n, key,
                                  smaller, bigger, indent+2);
            } 
          } 
        } else {
          // The new key is the one that goes up
          // The left-most smaller pointer of the new node
          // is what was bigger than the key that is inserted
          debug(indent, "moving up the new key");
          new_n->k[0].bigger = bigger;
          if (new_n->k[0].bigger) {
            (new_n->k[0].bigger)->parent = new_n;
          }
          return insert_in_node(n->parent, key_up,
                                n, new_n, indent+2);
        }
      } else {
        // There is still room in the node
        if (pos <= n->keycnt) {
          debug(indent, "shifting %d elements from %hd to %hd in node %hd",
                (1 + n->keycnt - pos),
                pos, pos + 1, n->id);
          // (dest, src, size)
          (void)memmove(&(n->k[pos+1]), &(n->k[pos]),
                        sizeof(REDIRECT_T) * (1 + n->keycnt - pos));
        }
        n->k[pos].key = key;
        if (!n->k[pos-1].bigger) {
          n->k[pos-1].bigger = smaller;
          if (smaller) {
            smaller->parent = n;
          }
        }
        n->k[pos].bigger = bigger;
        if (bigger) {
          // Adjust
          bigger->parent = n;
        }
        (n->keycnt)++;
        if (debugging()) {
          debug_no_nl(indent, "updated node %d ", n->id);
          btree_show_node(n);
        }
      }
    } else {
      return -1;
    }
  } 
  return 0;
}

static short insert_key(NODE_T  *n,
                        char    *key,
                        short    indent) {
    // -1 if there is something wrong, 0 if OK
    short pos = 1;
    int   cmp = -1;

    assert(key && n);
    // Find the leaf node where the key should be stored
    if (debugging()) {
      debug_no_nl(indent, "searching node %hd: ", n->id);
      btree_show_node(n);
    }
    while ((pos <= n->keycnt)
           && ((cmp = btree_keycmp(key, n->k[pos].key)) > 0)) {
      pos++;
    }
    if (cmp == 0) {
      // We've found it in the tree
      debug(indent, "** found at position %hd", pos);
      if (btree_unique()) {
        debug(indent, "duplicates not allowed");
        return -1;
      }
    } else {
      // We have found a key that is greater or we have reached
      // the end of the node
      if (!_is_leaf(n)) {
        return insert_key(n->k[pos-1].bigger, key, indent+2);
      } else {
        char *k;
        debug(indent, "should go in this leaf node");
        k = key_duplicate(key);
        return insert_in_node(n, k, NULL, NULL, indent);
      }
    }
  return -1;
}

static int insert_from_root(char *key, int indent) {
   NODE_T *n;
   int     ret;

   if (!btree_root()) {
     debug(indent, "insert_from_root() - creating root");
     n = new_node(NULL);
     btree_setroot(n);
   }
   ret = insert_key(btree_root(), key, indent);
   /*
   if (debugging()) {
     if (btree_check(btree_root(), (char *)NULL)) {
       btree_display(btree_root(), 0);
       assert(0);  // Force exit
     }
   }
   */
   return ret;
}

extern int btree_insert(char *key) {
    int     val;

    if (btree_numeric()) {
      if (sscanf(key, "%d", &val) == 0) {
        fprintf(stdout, "%s: invalid numeric value\n", key);
        return -1;
      }
    } 
    if (btree_numeric()) {
      return insert_from_root((char *)&val, 0);
    } else {
      return insert_from_root(key, 0);
    }
    return -1;
}
