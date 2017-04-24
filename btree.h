#ifndef BTREE_H

#define BTREE_H

#define DEF_MAX_KEYS  4
#define DEF_FILL_RATE 0.5 

#define _is_leaf(n)  (n->k[0].bigger == NULL)
#define MIN_KEYS     (int)(btree_maxkeys() * btree_fillrate())

struct node_t;

// Convenience structure
typedef struct redirect_t {
            char          *key;
            struct node_t *bigger;
                     // Pointer to the subtree that contains
                     // keys bigger than the current one
                     // and smaller than the key to the right
        } REDIRECT_T;

typedef struct node_t {
          short           id;      // For educational purposes
          short           keycnt;
          REDIRECT_T     *k;       // 1 + maximum number of keys
          struct node_t  *parent;  // Helps when deleting
         } NODE_T;

// Convenience structure (key location)
typedef struct keyloc_t {
          NODE_T  *n;
          short    pos;
         } KEYLOC_T;

extern void     btree_setunique(void);
extern char     btree_unique(void);
extern void     btree_setnumeric(void);
extern char     btree_numeric(void);
extern void     btree_setmaxkeys(short n);
extern short    btree_maxkeys(void);
extern float    btree_fillrate(void);
extern NODE_T  *btree_root(void);
extern void     btree_setroot(NODE_T *n);
extern int      btree_insert(char *key);
extern int      btree_delete(char *key);
extern void     btree_search(char *key);
extern void     btree_free(void);
extern void     btree_show_node(NODE_T *n);
extern void     btree_display(NODE_T *n, int blanks);
extern int      btree_keycmp(char *k1, char *k2);
extern KEYLOC_T btree_find_key(char *key);
// For debugging
extern char     btree_check(NODE_T *n, char *prev_key);


extern char    *key_duplicate(char *key);
extern NODE_T  *merge_leaf_nodes(NODE_T *left, char *sep_key,
                                 NODE_T *right, short lvl);
extern NODE_T  *new_node(NODE_T *parent);
extern NODE_T  *left_sibling(NODE_T *n, short *sep_pos);
extern NODE_T  *right_sibling(NODE_T *n, short *sep_pos);
extern NODE_T  *find_node(NODE_T *tree, char *key);
extern short    find_pos(NODE_T *n, char *key, char present, short lvl);

#endif
