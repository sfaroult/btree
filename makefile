CFLAGS=-Wall
OBJFILES= btree.o btree_op.o btree_ins.o btree_del.o btree_search.o \
		  bt.o debug.o
#LIBS= -lefence

all: btree

btree: $(OBJFILES)
	gcc -o btree $(OBJFILES) $(LIBS)

%.o:%.c
	gcc $(CFLAGS) -c -g $< -o $@

clean:
	-rm btree
	-rm *.o
