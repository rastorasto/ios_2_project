#
#  Name: Makefile
#  Author: xuhliar00
#  Created: 26.04.2024
#
CC = gcc
CFLAGS = -g -std=gnu99 -Wall -Wextra -Werror -pedantic

all: proj2

proj2.o: proj2.c

proj2: proj2.o
	$(CC) $(CFLAGS) proj2.o -o proj2 -lpthread

clean:
	rm -f proj2 proj2.o proj2.zip proj2.out

zip:
	zip proj2.zip *.c *.h Makefile