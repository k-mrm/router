CC = gcc
LD = ld

CFLAGS += -Wall -Wextra -I ./ -Og -g

TARGET = router
OBJS = arp.o ether.o icmp.o ipv4.o main.o net.o route.o

.PHONY: router clean

router: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.out $(OBJS) $(TARGET) $(COBJS) $(SOBJS)
