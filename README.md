# btree
This is a command-line tool for demoing B-trees during a class (data structures, database concepts ...). The implementation isn't standard (each node for instance contains a pointer back to its parent), coding isn't necessarily optimal but it's robust - it has been tested successfully, with a various number of keys per node, on the random insertion of 10,000 values that were later all randomly deleted.
 The default number of keys is 4, which can be changed with the -k <value> flag when invoking the program (type ./btree -? for available flags). One feature that can be interesting, especially if short on time, is the pre-loading of the B-Tree with values read from a file, so as to jump immediately to the interesting bits.
 Note also that keys are expected to be strings by default (easier to read or guess from a distance IMHO). If you want to use integer values, popular with text books, you must use the -n flag to get a numerical ordering of keys.
A command line interface allows to add and remove keys, and to display the content of the B-Tree at will. The HELP command lists everything available.
The TRC command turns on extensive tracing, that lists (with indention where the program recurses) all performed operations. It's turned off with NOTRC.

Here are a few technical details:
 - the main parameter is the maximum number of keys in a node, which I find easier to understand for students than an "order" or "degree". If this number K is even, each node will contain between K/2 and K keys. If it's odd, each node will contain between (K-1)/2 and K keys.
 - insertion is always first inside a leaf node. If the node is full, it's split at the middle (or, with an even number of keys, at the position that will ensure an equal number of keys in the two sibling nodes once the new key has been inserted), and the key at the split position is pushed up to the parent node. This can be recursive.
 - physical deletion is always, ultimately, to a leaf.
   When we are deleting a key from a leaf, if the number of keys is less than the minimum then the left sibling node, if there is one, is checked. If it contains more than the minimum number of keys, a key is borrowed by a rotation that involves the parent node. If this doesn't work, the right sibling is checked for a similar operation. If none of these options work, two neighboring nodes are merged and the key that separates them in the parent node is brought down in between. This key is then removed from the parent node.
   When we are deleting a key from an internal node, as required by a merge, we first try to replace it with the preceding key value (necessarily in a leaf, it's easy to prove) if belongs to a node with more than the minimum number of keys. Failing that, we try to replace with the following key. Failing that, we replace the node by the preceding key and recursively call key removal, this time from an internal node.

Have fun.
