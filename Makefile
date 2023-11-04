CC = gcc
LD = ld

CFLAGS += -Wall -Wextra -I ./ -Og

TARGET = router
OBJS = arp.o ether.o ipv4.o main.o net.o

.PHONY: router clean

router: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.out $(OBJS) $(TARGET) $(COBJS) $(SOBJS)
