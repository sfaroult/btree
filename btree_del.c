/* ----------------------------------------------------------------- *
 *
 *                         btree_del.c
 *
 *  Deletion from a B-tree
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

// We use a special structure to memorize when we merge
// where everything was before merging 
typedef struct key_pos_t {
             NODE_T *n;
             short   pos;
             short   keycnt; 
            } KEY_POS_T;

// Forward declaration
static short delete_key(NODE_T    *n,
                        char      *key,
                        short indent);

static NODE_T *leaf_with_greatest_key(NODE_T *subtree) {
    if (subtree) {
      if (subtree->k[subtree->keycnt].bigger) {
        return leaf_with_greatest_key(subtree->k[subtree->keycnt].bigger);
      } else {
        return subtree;
      }
    }
    return NULL;
}

static NODE_T *leaf_with_smallest_key(NODE_T *subtree) {
    if (subtree) {
      if (subtree->k[0].bigger) {
        return leaf_with_smallest_key(subtree->k[0].bigger);
      } else {
        return subtree;
      }
    }
    return NULL;
}

static int borrow_from_left(NODE_T *n, short indent) {
   if (n && n->parent) {
      short   parent_pos;
      NODE_T *par = n->parent;
      NODE_T *l = left_sibling(n, &parent_pos);
      char   *k;

      // Check whether we can borrow a key from the left sibling
      if (l && (l->keycnt > MIN_KEYS)) {
        // Let's call K the key in the parent that
        // is greater than all keys in the left sibling
        // and smaller than all keys in the node that
        // underflows; its index in the parent will
        // be parent_pos (set when searching for the sibling 
        // nodes).
        // Move the greatest key of the left sibling
        // to the place of K in the parent, and
        // bring K as the first key in the current node.
        // --------------
        debug(indent, "borrowing from left node %hd", l->id);
        if (debugging()) {
          debug_no_nl(indent, "left node before borrowing: ");
          btree_show_node(l);
          debug_no_nl(indent, "current node %hd before borrowing: ", n->id);
          btree_show_node(n);
        }
        // Make room for K (dest, src, size)
        (void)memmove(&(n->k[1]), &(n->k[0]),
                      sizeof(REDIRECT_T) * (n->keycnt+1));
        // Add K
        n->k[1].key = par->k[parent_pos].key;
        // Values that precede K are the ones bigger
        // than the biggest key in the left node
        n->k[0].key = NULL;
        n->k[0].bigger = l->k[l->keycnt].bigger;
        if (n->k[0].bigger) {
          (n->k[0].bigger)->parent = n;
        }
        (n->keycnt)++;
        // Find the greatest key in the left sibling
        k = l->k[l->keycnt].key;
        // Cleanup
        l->k[l->keycnt].key = NULL;
        l->k[l->keycnt].bigger = NULL;
        (l->keycnt)--;
        // Store k in the parent
        par->k[parent_pos].key = k;
        if (debugging()) {
          debug_no_nl(indent, "left node after borrowing: ");
          btree_show_node(l);
          debug_no_nl(indent, "current node %hd after borrowing: ", n->id);
          btree_show_node(n);
        }
        return 0;
      } 
    }
    return -1;
}

static int borrow_from_right(NODE_T *n, short indent) {
   if (n && n->parent) {
      short   parent_pos;
      NODE_T *par = n->parent;
      NODE_T *r = right_sibling(n, &parent_pos);
      char   *k;

      // Check whether we can borrow a key from the right sibling
      if (r && (r->keycnt > MIN_KEYS)) {
        // Let's call K the key in the parent that
        // is smaller than all keys in the right sibling
        // and greater than all keys in the node that
        // underflows; its index in the parent will
        // be parent_pos (set when searching for the sibling 
        // nodes).
        // Move the smallest key of the right sibling
        // to the place of K in the parent, and
        // add K as the last key in the current node.
        // --------------
        debug(indent, "borrowing from right node %hd", r->id);
        if (debugging()) {
          debug_no_nl(indent, "current node %hd before borrowing: ", n->id);
          btree_show_node(n);
          debug_no_nl(indent, "right node before borrowing: ");
          btree_show_node(r);
        }
        // Add K
        (n->keycnt)++;
        n->k[n->keycnt].key = par->k[parent_pos].key;
        n->k[n->keycnt].bigger = r->k[0].bigger;
        if (n->k[n->keycnt].bigger) {
          (n->k[n->keycnt].bigger)->parent = n;
        }
        // Find the smallest key in the right sibling
        k = r->k[1].key;
        // Cleanup the right sibling - dest, src, size
        (void)memmove(&(r->k[0]), &(r->k[1]),
                      sizeof(REDIRECT_T) * r->keycnt);
        r->k[0].key = NULL;
        r->k[r->keycnt].key = NULL;
        r->k[r->keycnt].bigger = NULL;
        (r->keycnt)--;
        // Store k in the parent
        par->k[parent_pos].key = k;
        if (debugging()) {
          debug_no_nl(indent, "current node %hd after borrowing: ", n->id);
          btree_show_node(n);
          debug_no_nl(indent, "right node after borrowing: ");
          btree_show_node(r);
        }
        return 0;
      } 
    }
    return -1;
}

static short merge_nodes(NODE_T *left,
                         NODE_T *right,
                         NODE_T *par,  // parent of left and right
                         short   lvl) {
   // Note that this function doesn't remove the parent key
   // but it returns its position
   short i = 0;

   assert(left && right && par);
   debug(lvl, "merging left node %hd with right node %hd",
              left->id, right->id);
   if (debugging()) {
      debug_no_nl(lvl, "left before merge: ");
      btree_show_node(left);
      debug_no_nl(lvl, "right before merge: ");
      btree_show_node(right);
   }
   assert((left->keycnt + 1 + right->keycnt) <= btree_maxkeys());
   while ((i < par->keycnt)
          && (par->k[i].bigger != left) > 0) {
     i++;
   }
   assert((i < par->keycnt) && (par->k[i+1].bigger == right));
   i++; // We have stopped just before the key between left and right
   left->k[left->keycnt+1].key = par->k[i].key;
   left->k[left->keycnt+1].bigger = right->k[0].bigger;
   par->k[i].key = NULL;
   par->k[i].bigger = NULL;
   (left->keycnt)++;
   // (dest, src, size) 
   (void)memmove(&(left->k[left->keycnt+1]),
                 &(right->k[1]),
                 sizeof(REDIRECT_T) * right->keycnt);
   // Adjust parent pointer
   if (!_is_leaf(left)) {
     short k;
     for (k = left->keycnt - 1;
          k <= left->keycnt + right->keycnt;
          k++) {
       if (left->k[k].bigger) {
         (left->k[k].bigger)->parent = left;
       }
     }
   }
   left->keycnt += right->keycnt; 
   if (debugging()) {
      debug_no_nl(lvl, "left after merge: ");
      btree_show_node(left);
   }
   // Free the right node (except keys, moved)
   debug(lvl, "removing right node %hd after merge", right->id);
   free(right->k);
   free(right);
   return i;
}

static int delete_node(NODE_T *n,
                       short   pos,
                       short   indent) {
  assert(n && (pos > 0) && (pos <= n->keycnt));
  debug(indent, "removing key at position %hd from node %hd",
        pos, n->id);
  if (n->k[pos].key) {
    free(n->k[pos].key);
  }
  if (pos < n->keycnt) {
    // (dest, src, size)
    (void)memmove(&(n->k[pos]), &(n->k[pos+1]),
                  sizeof(REDIRECT_T) * (n->keycnt - pos));
  }
  n->k[n->keycnt].key = NULL;
  n->k[n->keycnt].bigger = NULL;
  (n->keycnt)--;
  if (debugging()) {
    debug_no_nl(indent, "node now contains: ");
    btree_show_node(n);
  }
  if (n->keycnt >= MIN_KEYS) {
    debug(indent,
          "still %hd key%s in it - success",
          n->keycnt, (n->keycnt > 1? "s" : ""));
    return 0; // Fine
  }
  if (n->parent == NULL) { // Root
    if (n->keycnt >= 1) {
      debug(indent,
            "removed one key from root (node %hd) - still %hd key%s in it",
            n->id,
            n->keycnt, (n->keycnt > 1? "s" : ""));
    } else {
      if (!_is_leaf(n)) {
        debug(indent,
              "node %hd deleted - new root node %hd",
              n->id, (n->k[0].bigger)->id);
        btree_setroot(n->k[0].bigger);
      } else {
        debug(indent, "*** Tree emptied ***");
        btree_setroot(NULL);
      }
      free(n->k);
      free(n);
    }
    return 0;  // Fine
  }
  // Underflow
  debug(indent, "underflow detected");
  // Try to borrow a key from the left
  if (borrow_from_left(n, indent)) {
    // If it fails borrow from right
    if (borrow_from_right(n, indent)) {
      // If it fails merge with left node, but
      // then we must recurse
      short   parent_pos = 0;
      NODE_T *par = n->parent;
      NODE_T *l = left_sibling(n, &parent_pos);

      if (l) {
         debug(indent, "merging with left node");
         parent_pos = merge_nodes(l, n, par, indent);
      } else {
         NODE_T *r = right_sibling(n, &parent_pos);
         debug(indent, "merging with right node");
         parent_pos = merge_nodes(n, r, par, indent);
      }
      debug(indent, "remove key at position %hd from parent %hd",
                    parent_pos, par->id);
      // After merging one node must be removed from the 
      // parent - recurse
      return delete_node(par, parent_pos, indent+2);
    } else {
      return 0;
    }
  } else {
    return 0;
  }
  return -1;
}

static short delete_key(NODE_T    *n,
                        char      *key,
                        short indent) {
    // -1 if there is something wrong, 0 if OK
    // Find where the key is in the tree.
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
      if (_is_leaf(n)) {
        debug(indent, "removing from leaf node");
        if (debugging()) {
          debug_no_nl(indent, "before calling delete_node:");
          btree_show_node(n);
        }
        return delete_node(n, pos, indent);
      } else {
        debug(indent, "removing from internal node");
        // Note that the previous key and the succeeding key of
        // a key inside an internal node are always in a leaf.
        //
        // "Real" deletions are always from leaves, in the same
        // way that "real" insertions are always into leaves.
        debug(indent, "finding surrounding keys");
        NODE_T *prev = leaf_with_greatest_key(n->k[pos-1].bigger);
        NODE_T *next;

        if (prev) {
          if (btree_numeric()) {
            debug(indent, "previous key %d in leaf node %hd",
                *((int*)(prev->k[prev->keycnt].key)),
                prev->id);
          } else {
            debug(indent, "previous key %s in leaf node %hd",
                  prev->k[prev->keycnt].key,
                prev->id);
        }
        if (prev->keycnt > MIN_KEYS) {
          // No problem, just replace the key to remove with 
          // the previous key, which will be removed from its
          // leaf
          debug(indent, "simple replacement with the previous key");
          free(n->k[pos].key);
          n->k[pos].key = prev->k[prev->keycnt].key;
          prev->k[prev->keycnt].key = NULL;
          (prev->keycnt)--;
          debug(indent, "removal successful");
          return 0;   // Neat and clean - no need for a recursive call
        }
      } else {
        // Should not happen
        debug(indent, "Couldn't find previous key");
        return -1;
      }
      // Not enough keys on the left. Try the right.
      next = leaf_with_smallest_key(n->k[pos].bigger);
      if (next) {
        if (btree_numeric()) {
          debug(indent, "next key %d in leaf node %hd",
                *((int*)(next->k[1].key)),
                next->id);
        } else {
          debug(indent, "next key %s in leaf node %hd",
                next->k[1].key,
                next->id);
        }
        if (next->keycnt > MIN_KEYS) {
          // Replace the key to remove with the next key, which
          // will be removed from its leaf
          debug(indent, "simple replacement with the next key");
          free(n->k[pos].key);
          n->k[pos].key = next->k[1].key;
          // Shift everything in the node where the next key 
          // used to be
          // (dest, src, size)
          (void)memmove(&(next->k[1]), &(next->k[2]),
                    sizeof(REDIRECT_T) * (next->keycnt - 1));
          next->k[next->keycnt].key = NULL;
          (next->keycnt)--;
          debug(indent, "removal successful");
          return 0;
        }
      } else {
        // Should not happen
        debug(indent, "Couldn't find next key");
        return -1; 
      }
      // Both the leaf where the previous key is located
      // and the leaf where the next key is located contain
      // the minimum number of keys.
      // In that case, we replace the key to delete
      // in the internal node with the previous key
      // (once again, in a leaf) but we call recursively
      // the function to trigger housekeeping operations
      // (i.e. underflow fixing) in the leaf where the previous
      // key was.
      if (prev) {
        debug(indent, "replacement with the previous key");
        free(n->k[pos].key);
        n->k[pos].key = prev->k[prev->keycnt].key;
        prev->k[prev->keycnt].key = NULL;
        // Call the removal of this key
        return delete_node(prev, prev->keycnt, indent+2); 
      }
    }
  } else {
    // Key not found in this node
    if (_is_leaf(n)) {
      return -1;  // Not in the tree
    }
    // cmp = btree_keycmp(key, n->k[pos].key)) > 0)
    if (cmp < 0) {
      // The key at 'pos' is bigger than the searched one
      return delete_key(n->k[pos-1].bigger, key, indent+2);
    } else {
      // The search key is bigger than all keys in the node
      return delete_key(n->k[n->keycnt].bigger, key, indent+2);
    }
  }
  debug(indent, "removal failed");
  return -1;
}

extern int btree_delete(char *key) {
    int      val;
    int      ret;

    if (btree_numeric()) {
      if (sscanf(key, "%d", &val) == 0) {
        fprintf(stdout, "Tree only contains numerical values\n");
        return -1;
      }
    }
    if (btree_numeric()) {
      ret = delete_key(btree_root(), (char *)&val, 0);
    } else {
      ret = delete_key(btree_root(), key, 0);
    }
    return ret;
}
