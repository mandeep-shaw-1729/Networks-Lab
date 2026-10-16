CC = gcc
CFLAGS = -Wall -Wextra -O3
COMMON_SRCS = frame.c crc.c checksum.c fileio.c

all: sender receiver test_eval

sender: sender.c error.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o sender sender.c error.c $(COMMON_SRCS)

receiver: receiver.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o receiver receiver.c $(COMMON_SRCS)

test_eval: test_eval.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o test_eval test_eval.c $(COMMON_SRCS)

test: test_eval
	./test_eval

demo: sender receiver
	./run_socket_demo.sh

report:
	pdflatex -interaction=nonstopmode report.tex && pdflatex -interaction=nonstopmode report.tex

clean:
	rm -f sender receiver test_eval print_collisions *.log *.aux *.log *.out *.toc *.synctex.gz
